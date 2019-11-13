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
    Utility::Arguments parser;
    parser
        //.addArgument("file").setHelp("file", "file to load")
        .addSkippedPrefix("magnum", "engine-specific options") // ignore --magnum- options
        .setGlobalHelp("Checkered rendering experiment.");
    parser.parse(arguments.argc, arguments.argv);

    // UI

    const char* fontName = "resources/fonts/Roboto-Regular.ttf";
    setFont(fontName, 15.0f);

    // Scene

    cameraObject
        .setParent(&scene)
        .translate(Vector3::zAxis(3.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));
    (*camera)
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(90.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(framebufferSize());

    manipulator.setParent(&scene);

    meshShader
        .setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0xffffff_rgbf)
        .setShininess(80.0f);

    lightPos = { -3.0f, 10.0f, 10.0f };

    const char* sceneFile = "resources/models/Suzanne.glb";
    loadScene(sceneFile, manipulator);

    // Framebuffers

    const Vector2i size = framebufferSize();
    CORRADE_INTERNAL_ASSERT(size % 2 == Vector2i(0, 0));

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
    animables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());

    // Ubuntu purple (Mid aubergine)
    // https://design.ubuntu.com/brand/colour-palette/
    GL::Renderer::setClearColor(0x5E2750_rgbf);

    GL::Framebuffer& framebuffer = framebuffers[currentFramebuffer];

    // render scene at half resolution

    framebuffer.bind();
    framebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    camera->draw(drawables);

    // blit with interpolation
    GL::Framebuffer::blit(framebuffer, GL::defaultFramebuffer, GL::defaultFramebuffer.viewport(), GL::FramebufferBlit::Color);

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
        bool movingCamera = true;
        ImGui::Checkbox("Animated objects", &animatedObjects);
        ImGui::Checkbox("Moving camera", &movingCamera);
        const ImVec2 margin = { 5, 5 };
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImVec2 pos = { screen.x - size.x - margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    ImGui::End();

    for(size_t i = 0; i < animables.size(); i++)
    {
        animables[i].setState(animatedObjects ? SceneGraph::AnimationState::Running : SceneGraph::AnimationState::Paused);
    }
}

bool Application::loadScene(const char* file, Object3D& parent)
{
    // TODO clear

    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnySceneImporter");
    if(!importer || !importer->openFile(file))
        return false;

    if(importer->defaultScene() == -1)
        return false;

    meshes = Containers::Array<Containers::Optional<GL::Mesh>>{ importer->mesh3DCount() };
    for(UnsignedInt i = 0; i < importer->mesh3DCount(); i++)
    {
        Containers::Optional<Trade::MeshData3D> meshData = importer->mesh3D(i);
        if(meshData && meshData->hasNormals() && GL::meshPrimitive(meshData->primitive()) == GL::MeshPrimitive::Triangles)
        {
            meshes[i] = MeshTools::compile(*meshData);
        }
    }
    
    Containers::Optional<Trade::SceneData> sceneData = importer->scene(importer->defaultScene());
    if(!sceneData)
        return false;

    // Add direct children
    for(UnsignedInt objectId : sceneData->children3D())
    {
        // TODO recursive
        Containers::Pointer<Trade::ObjectData3D> objectData = importer->object3D(objectId);

        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 && meshes[objectData->instance()])
        {
            // TODO load materials and material data
            SceneGraph::Drawable3D* drawable = new Drawable(object, meshShader, *meshes[objectData->instance()], lightPos, 0xffffff_rgbf);
            drawables.add(*drawable);
            SceneGraph::Animable3D* animable = new Animable(object);
            animables.add(*animable);
            animable->setState(SceneGraph::AnimationState::Running);
        }
    }

    return true;
}
