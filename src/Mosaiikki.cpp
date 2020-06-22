#include "Mosaiikki.h"

#include "Feature.h"
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/PluginManager/Manager.h>

using namespace Magnum;
using namespace Corrade;
using namespace Magnum::Math::Literals;
using namespace Feature;

Mosaiikki::Mosaiikki(const Arguments& arguments) :
    ImGuiApplication(arguments, NoCreate),
    logFile(std::string(NAME) + ".log", std::fstream::out | std::fstream::trunc),
    _debug(nullptr),
    _warning(nullptr),
    _error(nullptr),
    velocityShader(NoCreate),
    meshShader(NoCreate),
    velocityFramebuffer(NoCreate),
    velocityAttachment(NoCreate),
    velocityDepthAttachment(NoCreate),
    framebuffers { GL::Framebuffer(NoCreate), GL::Framebuffer(NoCreate) },
    colorAttachments(NoCreate),
    depthAttachments(NoCreate),
    currentFrame(0),
    depthBlitShader(NoCreate),
    reconstructionShader(NoCreate)
{
    // Configuration and GL context

    Configuration conf;
    conf.setSize({ 800, 600 });
    conf.setTitle(NAME);
    conf.setWindowFlags(Configuration::WindowFlag::Resizable);

    GLConfiguration glConf;
#ifdef CORRADE_IS_DEBUG_BUILD
    glConf.addFlags(GLConfiguration::Flag::Debug);
#endif
    create(conf, glConf);

#ifndef CORRADE_IS_DEBUG_BUILD
    setSwapInterval(0); // disable v-sync
#endif

    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL::Version::GL320);

    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::explicit_attrib_location); // core in 3.3
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::sample_shading);           // core in 4.0
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::texture_multisample);      // core in 3.2

    // ARB extension is really only supported by Nvidia (Maxwell and later) and requires GL 4.5
    bool ext_arb = GL::Context::current().isExtensionSupported<GL::Extensions::ARB::sample_locations>();
    bool ext_nv = GL::Context::current().isExtensionSupported<GL::Extensions::NV::sample_locations>();
    bool ext_amd = GL::Context::current().isExtensionSupported<GL::Extensions::AMD::sample_positions>();
    // TODO Intel? Can't find a supported GL extension although D3D12 support for it exists

    CORRADE_ASSERT(ext_arb || ext_nv || ext_amd, "No extension for setting sample positions found", );

    // Debug output

    if(logFile.good())
    {
        _debug = Corrade::Containers::pointer<Utility::Debug>(&logFile, Utility::Debug::Flag::NoSpace);
        _warning = Corrade::Containers::pointer<Utility::Warning>(&logFile, Utility::Debug::Flag::NoSpace);
        _error = Corrade::Containers::pointer<Utility::Error>(&logFile, Utility::Debug::Flag::NoSpace);
    }

    profiler.setup(DebugTools::GLFrameProfiler::Value::FrameTime | DebugTools::GLFrameProfiler::Value::GpuDuration, 60);

#ifdef CORRADE_IS_DEBUG_BUILD
    // redirect debug messages to default Corrade Debug output
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
    parser.addOption("mesh", "resources/models/Suzanne.glb")
        .setHelp("mesh", "mesh to load")
        .addOption("font", "resources/fonts/Roboto-Regular.ttf")
        .setHelp("font", "GUI font to load")
        .addSkippedPrefix("magnum", "engine-specific options") // ignore --magnum- options
        .setGlobalHelp("Checkered rendering experiment.");
    parser.parse(arguments.argc, arguments.argv);

    // UI

    std::string fontFile = parser.value<std::string>("font");
    setFont(fontFile.c_str(), 15.0f);

    // Scene

    lightPos = { -3.0f, 10.0f, 10.0f };

    cameraObject.setParent(&scene).translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));
    camera->setViewport(framebufferSize());
    updateProjectionMatrix(*camera);

    SceneGraph::Animable3D* animable = new Animable3D(cameraObject, Vector3::yAxis(), 1.5f, 1.0f);
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

                for(Drawable3D* drawable : featuresInChildren<Drawable3D>(duplicated))
                {
                    drawable->setColor(Color4(i, j, k) * 1.0f / objectGridSize);

                    VelocityDrawable3D* velocityDrawable =
                        new VelocityDrawable3D(duplicated, velocityShader, drawable->getMesh());
                    velocityDrawables.add(*velocityDrawable);
                }

                SceneGraph::Animable3D* duplicatedAnimable = new Animable3D(duplicated, Vector3::xAxis(), 3.0f, 5.5f);
                meshAnimables.add(*duplicatedAnimable);
                duplicatedAnimable->setState(SceneGraph::AnimationState::Running);
            }
        }
    }

    for(Drawable3D* drawable : featuresInChildren<Drawable3D>(original))
    {
        drawables.remove(*drawable);
    }

    // Framebuffers

    resizeFramebuffers(framebufferSize());

    GL::Renderer::enable(GL::Renderer::Feature::Multisampling);
    GL::Renderer::enable(GL::Renderer::Feature::SampleShading);

    framebuffers[0].bind();

    // set sample locations

    // default on Nvidia 1070:
    // 0: (0.75, 0.75)
    // 1: (0.25, 0.25)
    // seems to be the opposite of the D3D11 pattern
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels

    const GLsizei SAMPLE_COUNT = 2;
    float samplePositions[SAMPLE_COUNT][2] = { { 0.75f, 0.75f }, { 0.25f, 0.25f } };

    if(ext_arb)
    {
        glFramebufferSampleLocationsfvARB(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, &samplePositions[0][0]);
    }
    else if(ext_nv)
    {
        glFramebufferSampleLocationsfvNV(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, &samplePositions[0][0]);
    }
    else if(ext_amd)
    {
        for(GLuint i = 0; i < SAMPLE_COUNT; i++)
        {
            glSetMultisamplefvAMD(GL_SAMPLE_POSITION, i, &samplePositions[i][0]);
        }
    }

    // read and report sample locations
    // just a sanity check, really
    for(GLuint i = 0; i < SAMPLE_COUNT; i++)
    {
        // GL_SAMPLE_POSITION reads the default sample location with ARB_sample_locations
        // AMD_sample_positions doesn't have GL_PROGRAMMABLE_SAMPLE_LOCATION_ARB
        // does GL_SAMPLE_POSITION return the programmable or the default locations then?
        GLenum name = ext_arb ? GL_PROGRAMMABLE_SAMPLE_LOCATION_ARB
                              : (ext_nv ? GL_PROGRAMMABLE_SAMPLE_LOCATION_NV : GL_SAMPLE_POSITION);
        glGetMultisamplefv(name, i, samplePositions[i]);
    }

    Debug(Debug::Flag::NoSpace) << "MSAA 2x sample positions:" << Debug::newline << "(" << samplePositions[0][0] << ", "
                                << samplePositions[0][1] << ")" << Debug::newline << "(" << samplePositions[1][0]
                                << ", " << samplePositions[1][1] << ")";

    // Shaders

    velocityShader = VelocityShader();

    meshShader = Shaders::Phong();
    meshShader.setAmbientColor(0x111111_rgbf).setSpecularColor(0xffffff_rgbf).setShininess(80.0f);
    meshShader.setLabel("Phong shader");

    depthBlitShader = DepthBlitShader();
    depthBlitShader.setLabel("Depth blit shader");

    reconstructionShader = ReconstructionShader();
    reconstructionShader.setLabel("Checkerboard resolve shader");

    timeline.start();
}

void Mosaiikki::updateProjectionMatrix(Magnum::SceneGraph::Camera3D& cam)
{
    constexpr float near = 0.5f;
    constexpr float far = 50.0f;
    //constexpr Rad hFOV_4by3 = 90.0_degf;
    //Rad vFOV = Math::atan(Math::tan(hFOV_4by3 * 0.5f) / (4.0f / 3.0f)) * 2.0f;
    constexpr Rad vFOV = 73.74_degf;

    float aspectRatio = Vector2(cam.viewport()).aspectRatio();
    Rad hFOV = Math::atan(Math::tan(vFOV * 0.5f) * aspectRatio) * 2.0f;
    cam.setProjectionMatrix(Matrix4::perspectiveProjection(hFOV, aspectRatio, near, far));
}

void Mosaiikki::resizeFramebuffers(Vector2i size)
{
    // make texture dimensions multiple of two
    size += size % 2;

    // xy = velocity, z = mask for dynamic objects
    velocityAttachment = GL::Texture2D();
    velocityAttachment.setStorage(1, GL::TextureFormat::RGBA16F, size);
    velocityAttachment.setLabel("Velocity texture");
    velocityDepthAttachment = GL::Texture2D();
    velocityDepthAttachment.setStorage(1, GL::TextureFormat::DepthComponent24, size);
    velocityDepthAttachment.setLabel("Velocity depth texture");

    velocityFramebuffer = GL::Framebuffer({ { 0, 0 }, size });
    velocityFramebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), velocityAttachment, 0);
    velocityFramebuffer.attachTexture(GL::Framebuffer::BufferAttachment::Depth, velocityDepthAttachment, 0);

    CORRADE_INTERNAL_ASSERT(velocityFramebuffer.checkStatus(GL::FramebufferTarget::Read) ==
                            GL::Framebuffer::Status::Complete);
    CORRADE_INTERNAL_ASSERT(velocityFramebuffer.checkStatus(GL::FramebufferTarget::Draw) ==
                            GL::Framebuffer::Status::Complete);

    const Vector2i quarterSize = size / 2;
    const Vector3i arraySize = { quarterSize, FRAMES };

    colorAttachments = GL::MultisampleTexture2DArray();
    colorAttachments.setStorage(2, GL::TextureFormat::RGBA8, arraySize, GL::MultisampleTextureSampleLocations::Fixed);
    colorAttachments.setLabel("Color texture array (quarter-res 2x MSAA)");
    depthAttachments = GL::MultisampleTexture2DArray();
    depthAttachments.setStorage(
        2, GL::TextureFormat::DepthComponent24, arraySize, GL::MultisampleTextureSampleLocations::Fixed);
    depthAttachments.setLabel("Depth texture array (quarter-res 2x MSAA)");

    for(size_t i = 0; i < FRAMES; i++)
    {
        framebuffers[i] = GL::Framebuffer({ { 0, 0 }, quarterSize });
        framebuffers[i].attachTextureLayer(GL::Framebuffer::ColorAttachment(0), colorAttachments, i);
        framebuffers[i].attachTextureLayer(GL::Framebuffer::BufferAttachment::Depth, depthAttachments, i);

        std::string label = Utility::format("Framebuffer {} (quarter-res)", i + 1);
        framebuffers[i].setLabel(label);

        CORRADE_INTERNAL_ASSERT(framebuffers[i].checkStatus(GL::FramebufferTarget::Read) ==
                                GL::Framebuffer::Status::Complete);
        CORRADE_INTERNAL_ASSERT(framebuffers[i].checkStatus(GL::FramebufferTarget::Draw) ==
                                GL::Framebuffer::Status::Complete);
    }
}

void Mosaiikki::drawEvent()
{
    profiler.beginFrame();

    meshAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());
    cameraAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());

    constexpr GL::Renderer::DepthFunction depthFunction = GL::Renderer::DepthFunction::LessOrEqual; // default: Less

    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::setDepthFunction(depthFunction);

    const Matrix4 unjitteredProjection = camera->projectionMatrix();

    static Array<FRAMES, Matrix4> oldMatrices = { Matrix4(Math::IdentityInit), Matrix4(Math::IdentityInit) };
    Array<FRAMES, Matrix4> matrices;
    // jitter viewport half a pixel to the right = one pixel in the full-res framebuffer
    // = width of NDC divided by full-res pixel count
    const float offset = 2.0f / camera->viewport().x();
    matrices[JITTERED_FRAME] = Matrix4::translation(Vector3::xAxis(offset)) * unjitteredProjection;
    matrices[1 - JITTERED_FRAME] = unjitteredProjection;

    // fill velocity buffer

    if(options.reconstruction.createVelocityBuffer)
    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 0, "Velocity buffer");

        velocityFramebuffer.bind();
        velocityFramebuffer.clearColor(0, 0_rgbf);
        velocityFramebuffer.clearDepth(1.0f);

        // use current frame's jitter
        // this only matters because we blit the velocity frame buffer to reuse it for the quarter resolution pass
        // without it, you can use either jittered or unjittered, as long as they match
        velocityShader.setProjection(matrices[currentFrame]).setOldProjection(oldMatrices[currentFrame]);

        camera->draw(velocityDrawables);
    }

    // render scene at quarter resolution

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 0, "Scene rendering (quarter-res)");

        GL::Framebuffer& framebuffer = framebuffers[currentFrame];
        framebuffer.bind();

        const Color3 clearColor = 0x111111_rgbf;
        framebuffer.clearColor(0, clearColor);

        // copy and reuse velocity depth buffer
        if(options.reconstruction.createVelocityBuffer && options.reuseVelocityDepth)
        {
            GL::Renderer::setDepthFunction(
                GL::Renderer::DepthFunction::Always); // always pass depth test for fullscreen triangle
            GL::Renderer::setColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // disable color writing

            // blit to quarter res with max filter
            depthBlitShader.bindDepth(velocityDepthAttachment);
            depthBlitShader.draw();

            GL::Renderer::setColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // enable color writing
            GL::Renderer::setDepthFunction(depthFunction);                  // restore depth test
        }
        else
        {
            framebuffer.clearDepth(1.0f);
        }

        // run fragment shader for each sample
        GL::Renderer::enable(GL::Renderer::Feature::SampleShading);
        GL::Renderer::setMinSampleShading(1.0f);

        // TODO
        // force interpolation to each sample position, not the fragment center
        // https://www.khronos.org/opengl/wiki/Type_Qualifier_(GLSL)
        // does sample shading already do this?
        // this requires us to roll our own (Phong) shader

        // jitter camera if necessary
        camera->setProjectionMatrix(matrices[currentFrame]);
        camera->draw(drawables);

        GL::Renderer::disable(GL::Renderer::Feature::SampleShading);
    }

    
    // undo any jitter
    camera->setProjectionMatrix(unjitteredProjection);

    GL::defaultFramebuffer.bind();

    // combine framebuffers

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 1, "Checkerboard resolve");

        GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

        reconstructionShader.bindColor(colorAttachments)
            .bindDepth(depthAttachments)
            .bindVelocity(velocityAttachment)
            .setCurrentFrame(currentFrame)
            .setCameraInfo(*camera)
            .setOptions(options.reconstruction);
        reconstructionShader.draw();
    }

    // render UI

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 2, "imgui");

        ImGuiApplication::drawEvent();
    }

    // housekeeping

    currentFrame = (currentFrame + 1) % FRAMES;

    oldMatrices = matrices;

    timeline.nextFrame();
    profiler.endFrame();

    swapBuffers();
    redraw();
}

void Mosaiikki::viewportEvent(ViewportEvent& event)
{
    resizeFramebuffers(event.framebufferSize());
    camera->setViewport(event.framebufferSize());
    updateProjectionMatrix(*camera);
    ImGuiApplication::viewportEvent(event);
}

void Mosaiikki::buildUI()
{
    //ImGui::ShowDemoWindow();

    const ImVec2 margin = { 5.0f, 5.0f };
    const ImVec2 screen = ImGui::GetIO().DisplaySize;

    ImGui::Begin(
        "Options", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::Checkbox("Animated objects", &options.scene.animatedObjects);
        ImGui::Checkbox("Animated camera", &options.scene.animatedCamera);

        ImGui::Separator();

        ImGui::Checkbox("Create velocity buffer", &options.reconstruction.createVelocityBuffer);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Calculate per-pixel velocity vectors instead of reprojecting the pixel position using the depth buffer");
        {
            ImGuiDisabledZone zone(!options.reconstruction.createVelocityBuffer);
            ImGui::Checkbox("Re-use velocity depth", &options.reuseVelocityDepth);
            if(ImGui::IsItemHovered())
                ImGui::SetTooltip("Downsample and re-use the velocity pass depth buffer for the quarter-res pass");
        }

        ImGui::Checkbox("Always assume occlusion", &options.reconstruction.assumeOcclusion);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Always assume an old pixel is occluded in the current frame if the pixel moved by more than a quarter-res pixel.\n"
                "If this is disabled, a more expensive check against the average surrounding depth value is used.");

        ImGui::Separator();

        float w = ImGui::CalcItemWidth() / 2.0f;
        ImGui::SetNextItemWidth(w);

        static const char* const debugSamplesOptions[] = { "Combined", "Even", "Odd (jittered)" };
        ImGui::Combo("Show samples",
                     (int*)&options.reconstruction.debug.showSamples,
                     debugSamplesOptions,
                     Containers::arraySize(debugSamplesOptions));

        {
            ImGuiDisabledZone zone(!options.reconstruction.createVelocityBuffer);
            ImGui::Checkbox("Show velocity buffer", &options.reconstruction.debug.showVelocity);
        }

        ImGui::Checkbox("Show debug colors", &options.reconstruction.debug.showColors);
        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            {
                static const ImVec4 red = { 1.0f, 0.0f, 0.0f, 1.0f };
                ImGui::ColorButton("red", red, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
                ImGui::SameLine();
                ImGui::Text("Old pixel was occluded");
            }
            {
                static const ImVec4 green = { 0.0f, 1.0f, 0.0f, 1.0f };
                ImGui::ColorButton("green", green, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
                ImGui::SameLine();
                ImGui::Text("Old pixel position is outside the screen");
            }
            {
                static const ImVec4 blue = { 0.0f, 0.0f, 1.0f, 1.0f };
                ImGui::ColorButton("blue", blue, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
                ImGui::SameLine();
                ImGui::Text("Old pixel information is missing (jitter was cancelled out)");
            }
            ImGui::EndTooltip();
        }

        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 pos = { screen.x - size.x - margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    }
    ImGui::End();

    ImGui::Begin("Statistics",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::Text("%s", profiler.statistics().c_str());

        ImVec2 pos = { margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    }
    ImGui::End();

    for(size_t i = 0; i < meshAnimables.size(); i++)
    {
        meshAnimables[i].setState(options.scene.animatedObjects ? SceneGraph::AnimationState::Running
                                                                : SceneGraph::AnimationState::Paused);
    }

    // TODO rotation instead of translation animation
    if(cameraAnimables.size() > 0)
        cameraAnimables[0].setState(options.scene.animatedCamera ? SceneGraph::AnimationState::Running
                                                                 : SceneGraph::AnimationState::Paused);
}

bool Mosaiikki::loadScene(const char* file, Object3D& parent)
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

    meshes = Containers::Array<Containers::Optional<GL::Mesh>>(importer->meshCount());
    for(UnsignedInt i = 0; i < importer->meshCount(); i++)
    {
        Containers::Optional<Trade::MeshData> data = importer->mesh(i);
        if(data && data->hasAttribute(Trade::MeshAttribute::Position) &&
           data->hasAttribute(Trade::MeshAttribute::Normal) &&
           GL::meshPrimitive(data->primitive()) == GL::MeshPrimitive::Triangles)
        {
            meshes[i] = MeshTools::compile(*data);
        }
    }

    // add objects recursively

    for(UnsignedInt objectId : sceneData->children3D())
    {
        addObject(*importer, objectId, parent);
    }

    return true;
}

void Mosaiikki::addObject(Trade::AbstractImporter& importer, UnsignedInt objectId, Object3D& parent)
{
    // meshes are compiled and accessible at this point
    // TODO materials

    Containers::Pointer<Trade::ObjectData3D> objectData = importer.object3D(objectId);
    if(objectData)
    {
        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 &&
           meshes[objectData->instance()])
        {
            SceneGraph::Drawable3D* drawable =
                new Drawable3D(object, meshShader, *meshes[objectData->instance()], lightPos, 0xffffff_rgbf);
            drawables.add(*drawable);
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object);
        }
    }
}

Mosaiikki::Object3D& Mosaiikki::duplicateObject(Object3D& object, Object3D& parent)
{
    CORRADE_INTERNAL_ASSERT(!object.isScene());

    Object3D& duplicate = parent.addChild<Object3D>();
    duplicate.setTransformation(object.transformation());

    Drawable3D* drawable = feature<Drawable3D>(object);
    if(drawable)
    {
        Drawable3D* newDrawable = new Drawable3D(*drawable, duplicate);
        drawables.add(*newDrawable);
    }

    for(Object3D& child : object.children())
    {
        duplicateObject(child, duplicate);
    }

    return duplicate;
}