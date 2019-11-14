#include "Application.h"

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/PluginManager/Manager.h>
#include <cassert>

using namespace Magnum;
using namespace Corrade;
using namespace Magnum::Math::Literals;

Application::Application(const Arguments& arguments) :
    ImGuiApplication(arguments, Configuration()
        .setTitle("calculi")
#ifndef CORRADE_TARGET_EMSCRIPTEN
        .setSize({ 800, 600 })
#endif
    ),
    framebuffers {
        GL::Framebuffer(NoCreate),
        GL::Framebuffer(NoCreate)
    },
    currentFramebuffer(0)
{
    // command line

    Utility::Arguments parser;
    parser
        .addOption("mesh", "resources/models/Suzanne.glb")
            .setHelp("mesh", "mesh to load")
        .addOption("font", "resources/fonts/Roboto-Regular.ttf")
            .setHelp("font", "font to load")
        .addSkippedPrefix("magnum", "engine-specific options") // ignore --magnum- options
        .setGlobalHelp("Checkered rendering experiment.");
    parser.parse(arguments.argc, arguments.argv);

    // UI

    std::string fontFile = parser.value<std::string>("font");
    setFont(fontFile.c_str(), 15.0f);

    // Scene

    cameraObject
        .setParent(&scene)
        .translate(Vector3::zAxis(3.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));
    (*camera)
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(90.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(framebufferSize());

    SceneGraph::Animable3D* animable = new Animable(cameraObject, Vector3::yAxis(), 1.5f, 0.5f);
    cameraAnimable.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    manipulator.setParent(&scene);

    meshShader
        .setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0xffffff_rgbf)
        .setShininess(80.0f);

    lightPos = { -3.0f, 10.0f, 10.0f };

    // Objects

    Object3D& original = manipulator.addChild<Object3D>();
    std::string sceneFile = parser.value<std::string>("mesh");
    loadScene(sceneFile.c_str(), original);

    for(size_t i = 1; i < objectCount; i++)
    {
        Object3D& duplicated = duplicateObject(original, *original.parent());
        duplicated.scale(Vector3(i * 2.0f));
        duplicated.translate({ 0.0f, 0.0f, -20.0f * i * i });
        // TODO set color
        // TODO set animable range
    }

    // Framebuffers

    const Vector2i size = framebufferSize();
    assert(size % 2 == Vector2i(0, 0));

    for (size_t i = 0; i < 2; i++)
    {
        framebuffers[i] = GL::Framebuffer({ { 0, 0 }, size / 2 });
        colorAttachments[i].setStorage(1, GL::TextureFormat::RGBA8, size);
        depthStencilAttachments[i].setStorage(GL::RenderbufferFormat::Depth24Stencil8, size);
        framebuffers[i].attachTexture(GL::Framebuffer::ColorAttachment(0), colorAttachments[i], 0);
        framebuffers[i].attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, depthStencilAttachments[i]);
    }

    timeline.start();
}

void Application::drawEvent()
{
    meshAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());
    cameraAnimable.step(timeline.previousFrameTime(), timeline.previousFrameDuration());

    // Ubuntu purple (Mid aubergine)
    // https://design.ubuntu.com/brand/colour-palette/
    const Color3 clearColor = 0x5E2750_rgbf;

    GL::Framebuffer& framebuffer = framebuffers[currentFramebuffer];

    // render scene at half resolution

    framebuffer.bind();
    framebuffer.clearColor(0, clearColor);
    framebuffer.clearDepth(1.0f);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    camera->draw(drawables);

    // blit with interpolation
    GL::Framebuffer::blit(framebuffer, GL::defaultFramebuffer, framebuffer.viewport(), GL::defaultFramebuffer.viewport(), GL::FramebufferBlit::Color, GL::FramebufferBlitFilter::Nearest);

    currentFramebuffer = (currentFramebuffer + 1) % 2;

    GL::defaultFramebuffer.bind();

    // combine framebuffers

    ImGuiApplication::drawEvent();

    timeline.nextFrame();
}

void Application::viewportEvent(ViewportEvent& event)
{
    camera->setViewport(event.framebufferSize());
}

void Application::buildUI()
{
    //ImGui::ShowTestWindow();

    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        static bool animatedObjects = true;
        static bool animatedCamera = true;
        ImGui::Checkbox("Animated objects", &animatedObjects);
        ImGui::Checkbox("Animated camera", &animatedCamera);
        const ImVec2 margin = { 5, 5 };
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImVec2 pos = { screen.x - size.x - margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    ImGui::End();

    for(size_t i = 0; i < meshAnimables.size(); i++)
    {
        meshAnimables[i].setState(animatedObjects ? SceneGraph::AnimationState::Running : SceneGraph::AnimationState::Paused);
    }

    if(cameraAnimable.size() > 0)
        cameraAnimable[0].setState(animatedCamera ? SceneGraph::AnimationState::Running : SceneGraph::AnimationState::Paused);
}

bool Application::loadScene(const char* file, Object3D& parent)
{
    // TODO clear

    // load importer

    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnySceneImporter");
    if(!importer)
        return false;

    // load scene

    if(!importer->openFile(file))
        return false;

    if(importer->sceneCount() == 0)
        return false;

    Int sceneId = importer->defaultScene();
    if(sceneId == -1)
        sceneId = 0;
    Containers::Optional<Trade::SceneData> sceneData = importer->scene(sceneId);
    if(!sceneData)
        return false;

    // extract and compile meshes

    meshes = Containers::Array<Containers::Optional<GL::Mesh>>(importer->mesh3DCount());
    for(UnsignedInt i = 0; i < importer->mesh3DCount(); i++)
    {
        Containers::Optional<Trade::MeshData3D> meshData = importer->mesh3D(i);
        if(meshData && meshData->hasNormals() && GL::meshPrimitive(meshData->primitive()) == GL::MeshPrimitive::Triangles)
        {
            meshes[i] = MeshTools::compile(*meshData);
        }
    }

    // add objects recursively

    for(UnsignedInt objectId : sceneData->children3D())
    {
        addObject(*importer, objectId, parent);
    }

    return true;
}

void Application::addObject(Trade::AbstractImporter& importer, UnsignedInt objectId, Object3D& parent)
{
    // meshes are compiled and accesible at this point
    // TODO materials

    Containers::Pointer<Trade::ObjectData3D> objectData = importer.object3D(objectId);
    if(objectData)
    {
        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 && meshes[objectData->instance()])
        {
            SceneGraph::Drawable3D* drawable = new Drawable(object, meshShader, *meshes[objectData->instance()], lightPos, 0xffffff_rgbf);
            drawables.add(*drawable);
            SceneGraph::Animable3D* animable = new Animable(object, Vector3::xAxis(), 1.0f, 2.0f);
            meshAnimables.add(*animable);
            animable->setState(SceneGraph::AnimationState::Running);
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object);
        }
    }
}

Application::Object3D& Application::duplicateObject(Object3D& object, Object3D& parent)
{
    assert(!object.isScene());

    Object3D& duplicate = parent.addChild<Object3D>();
    duplicate.setTransformation(object.transformation());

    for(Object3D::FeatureType& feature : object.features())
    {
        Drawable* drawable = dynamic_cast<Drawable*>(&feature);
        if(drawable)
        {
            Drawable* newDrawable = new Drawable(*drawable, duplicate);
            drawables.add(*newDrawable);
        }

        Animable* animable = dynamic_cast<Animable*>(&feature);
        if(animable)
        {
            Animable* newAnimable = new Animable(*animable, duplicate);
            meshAnimables.add(*newAnimable);
        }
    }

    for(Object3D& child : object.children())
    {
        duplicateObject(child, duplicate);
    }

    return duplicate;
}
