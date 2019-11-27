#include "Application.h"

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/PluginManager/Manager.h>
#include <cassert>

using namespace Magnum;
using namespace Corrade;
using namespace Magnum::Math::Literals;
using namespace Feature;

Application::Application(const Arguments& arguments) :
    ImGuiApplication(arguments, NoCreate),
    logFile("calculi.log", std::fstream::out | std::fstream::trunc),
    debug(nullptr),
    meshShader(NoCreate),
    framebuffers {
        GL::Framebuffer(NoCreate),
        GL::Framebuffer(NoCreate)
    },
    colorAttachments(NoCreate),
    depthAttachments(NoCreate),
    currentFrame(0),
    reconstructionShader(NoCreate)
{
    // Log

    if(logFile.good())
        debug = Corrade::Containers::pointer<Utility::Debug>(&logFile, Utility::Debug::Flag::NoSpace);
    else
        debug = Corrade::Containers::pointer<Utility::Debug>();

    // Configuration

    Configuration conf;
    conf
#ifndef CORRADE_TARGET_EMSCRIPTEN
        .setSize({ 800, 600 })
#endif
        .setTitle("calculi");
    GLConfiguration glConf;
    glConf.setVersion(GL::Version::GL320);
#ifdef CORRADE_IS_DEBUG_BUILD
    glConf.addFlags(GLConfiguration::Flag::Debug);
#endif
    create(conf, glConf);

    //setSwapInterval(0); // disable v-sync

    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::sample_shading); // core in 4.0
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::texture_multisample); // core in 3.2
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::explicit_attrib_location); // core in 3.3
    // really only supported by Nvidia
    //MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::sample_locations);

#ifdef CORRADE_IS_DEBUG_BUILD
    GL::DebugOutput::setDefaultCallback();
    // disable unimportant output
    // markers and groups are only used for RenderDoc
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::Marker, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PushGroup, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PopGroup, false);
    // Nvidia drivers: "Buffer detailed info"
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, { 131185 }, false);
#endif

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

    lightPos = { -3.0f, 10.0f, 10.0f };

    cameraObject
        .setParent(&scene)
        .translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));
    (*camera)
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(90.0_degf, 1.0f, 0.5f, 50.0f))
        .setViewport(framebufferSize());

    SceneGraph::Animable3D* animable = new Animable(cameraObject, Vector3::yAxis(), 1.5f, 1.0f);
    cameraAnimables.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    manipulator.setParent(&scene);

    // Objects

    Object3D& original = manipulator.addChild<Object3D>();
    std::string sceneFile = parser.value<std::string>("mesh");
    loadScene(sceneFile.c_str(), original);

    Vector3 center((float)(objectGridSize - 1) / 2.0f);

    for(size_t i = 0; i < objectGridSize; i++)
    {
        for(size_t j = 0; j < objectGridSize; j++)
        {
            for(size_t k = 0; k < objectGridSize; k++)
            {
                Object3D& duplicated = duplicateObject(original, *original.parent());
                duplicated.translate((Vector3(i, j, -(float)k) - center) * 4.0f);

                for(Drawable* drawable : featuresInChildren<Drawable>(duplicated))
                {
                    drawable->setColor(Color4(i, j, k) * 1.0f / objectGridSize);
                }

                SceneGraph::Animable3D* duplicatedAnimable = new Animable(duplicated, Vector3::xAxis(), 3.0f, 1.5f);
                meshAnimables.add(*duplicatedAnimable);
                duplicatedAnimable->setState(SceneGraph::AnimationState::Running);
            }
        }
    }

    for(Drawable* drawable : featuresInChildren<Drawable>(original))
    {
        drawables.remove(*drawable);
    }

    // Framebuffers

    const Vector2i size = framebufferSize();
    assert(size % 2 == Vector2i(0, 0));

    Vector2i halfSize = size / 2;
    Vector3i arraySize = { halfSize, FRAMES };

    colorAttachments = GL::MultisampleTexture2DArray();
    colorAttachments.setStorage(2, GL::TextureFormat::RGBA8, arraySize, GL::MultisampleTextureSampleLocations::Fixed);
    colorAttachments.setLabel("Color texture array (half-res 2x MSAA)");
    depthAttachments = GL::MultisampleTexture2DArray();
    depthAttachments.setStorage(2, GL::TextureFormat::DepthComponent32, arraySize, GL::MultisampleTextureSampleLocations::Fixed);
    depthAttachments.setLabel("Depth texture array (half-res 2x MSAA)");

    for(size_t i = 0; i < FRAMES; i++)
    {
        framebuffers[i] = GL::Framebuffer({ { 0, 0 }, halfSize });
        framebuffers[i].attachTextureLayer(GL::Framebuffer::ColorAttachment(0), colorAttachments, i);
        framebuffers[i].attachTextureLayer(GL::Framebuffer::BufferAttachment::Depth, depthAttachments, i);

        Containers::Array<char> label = Utility::format( "Framebuffer {} (half-res)", i + 1);
        framebuffers[i].setLabel(label.data());

        assert(framebuffers[i].checkStatus(GL::FramebufferTarget::Read) == GL::Framebuffer::Status::Complete);
        assert(framebuffers[i].checkStatus(GL::FramebufferTarget::Draw) == GL::Framebuffer::Status::Complete);
    }

    GL::Renderer::enable(GL::Renderer::Feature::Multisampling);
    GL::Renderer::enable(GL::Renderer::Feature::SampleShading);

    framebuffers[0].bind();

    // 0: (0.75, 0.75)
    // 1: (0.25, 0.25)
    // seems to be the opposite of the D3D11 pattern
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels
    float samplePositions[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    glGetMultisamplefv(GL_SAMPLE_POSITION, 0, samplePositions[0]);
    glGetMultisamplefv(GL_SAMPLE_POSITION, 1, samplePositions[1]);

    // TODO set sample locations ourselves
    // glFramebufferSampleLocationsfv
    // requires GL 4.5
    // need to update Magnum

    Debug(Debug::Flag::NoSpace)
        << "MSAA 2x sample positions:" << Debug::newline
        << "(" << samplePositions[0][0] << ", " << samplePositions[0][1] << ")" << Debug::newline
        << "(" << samplePositions[1][0] << ", " << samplePositions[1][1] << ")";

    // Shaders

    meshShader = Shaders::Phong();
    meshShader
        .setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0xffffff_rgbf)
        .setShininess(80.0f);
    meshShader.setLabel("Phong shader");

    reconstructionShader = ReconstructionShader();
    reconstructionShader.setDebugShowSamples(ReconstructionShader::DebugSamples::Disabled);
    reconstructionShader.setLabel("Checkerboard resolve shader");

    timeline.start();
}

void Application::drawEvent()
{
    meshAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());
    cameraAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());

    GL::Framebuffer& framebuffer = framebuffers[currentFrame];

    // jitter camera if necessary

    Matrix4 projectionMatrix = camera->projectionMatrix();
    if(currentFrame == JITTERED_FRAME)
    {
        // jitter viewport half a pixel to the right = one pixel in the combined framebuffer
        // width of NDC divided by pixel count
        const float offset = (2.0f / (camera->viewport().x() / 2.0f)) / 2.0f;
        Matrix4 jitteredMatrix = Matrix4::translation(Vector3::xAxis(offset)) * projectionMatrix;
        camera->setProjectionMatrix(jitteredMatrix);
    }

    // render scene at half resolution

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 0, "Scene rendering (half-res)");

        framebuffer.bind();
        const Color3 clearColor = 0x111111_rgbf;
        framebuffer.clearColor(0, clearColor);
        framebuffer.clearDepth(1.0f);

        GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
        GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

        // run fragment shader for each sample
        GL::Renderer::enable(GL::Renderer::Feature::SampleShading);
        GL::Renderer::setMinSampleShading(1.0f);

        camera->draw(drawables);

        GL::Renderer::disable(GL::Renderer::Feature::SampleShading);
        GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
    }

    // undo jitter

    camera->setProjectionMatrix(projectionMatrix);

    // combine framebuffers

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 1, "Checkerboard resolve");

        GL::defaultFramebuffer.bind();

        reconstructionShader
            .bindColor(colorAttachments)
            .bindDepth(depthAttachments)
            .setCurrentFrame(currentFrame)
            .setCameraInfo(*camera)
            .draw();
    }

    // render UI

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 2, "imgui");
        
        ImGuiApplication::drawEvent();
    }

    // housekeeping

    currentFrame = (currentFrame + 1) % FRAMES;
    timeline.nextFrame();

    swapBuffers();
    redraw();
}

void Application::viewportEvent(ViewportEvent& event)
{
    camera->setViewport(event.framebufferSize());
}

void Application::buildUI()
{
    //ImGui::ShowTestWindow();

    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        static bool animatedObjects = false;
        static bool animatedCamera = false;
        ImGui::Checkbox("Animated objects", &animatedObjects);
        ImGui::Checkbox("Animated camera", &animatedCamera);

        ImGui::Separator();

        float w = ImGui::CalcItemWidth() / 3.0f;
        ImGui::SetNextItemWidth(w);
        static int debugSamples = (int)ReconstructionShader::DebugSamples::Disabled;
        const char* const debugSamplesOptions[] = { "Disabled", "Even", "Odd" };
        ImGui::Combo("Show samples", &debugSamples, debugSamplesOptions, 3);

        const ImVec2 margin = { 5, 5 };
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImVec2 pos = { screen.x - size.x - margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    ImGui::End();

    reconstructionShader.setDebugShowSamples((ReconstructionShader::DebugSamples)debugSamples);

    for(size_t i = 0; i < meshAnimables.size(); i++)
    {
        meshAnimables[i].setState(animatedObjects ? SceneGraph::AnimationState::Running : SceneGraph::AnimationState::Paused);
    }

    // TODO rotation instead of translation animation
    if(cameraAnimables.size() > 0)
        cameraAnimables[0].setState(animatedCamera ? SceneGraph::AnimationState::Running : SceneGraph::AnimationState::Paused);
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
    // meshes are compiled and accessible at this point
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

    Drawable* drawable = feature<Drawable>(object);
    if(drawable)
    {
        Drawable* newDrawable = new Drawable(*drawable, duplicate);
        drawables.add(*newDrawable);
    }

    for(Object3D& child : object.children())
    {
        duplicateObject(child, duplicate);
    }

    return duplicate;
}
