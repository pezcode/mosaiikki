#include "Mosaiikki.h"

#include "Feature.h"
#include "MagnumShadersSampleInterpolationOverride.h"
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Containers/GrowableArray.h>
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
    coloredMaterialShader(NoCreate),
    texturedMaterialShader(NoCreate),
    paused(false),
    advanceOneFrame(false),
    hideUI(false),
    velocityFramebuffer(NoCreate),
    velocityAttachment(NoCreate),
    velocityDepthAttachment(NoCreate),
    velocityShader(NoCreate),
    currentFrame(0),
    framebuffers { GL::Framebuffer(NoCreate), GL::Framebuffer(NoCreate) },
    colorAttachments(NoCreate),
    depthAttachments(NoCreate),
    depthBlitShader(NoCreate),
    outputFramebuffer(NoCreate),
    outputColorAttachment(NoCreate),
    reconstructionShader(NoCreate)
{
    // Configuration and GL context

    Configuration conf;
    conf.setSize({ 800, 600 });
    conf.setTitle(NAME);
    conf.setWindowFlags(Configuration::WindowFlag::Resizable);

    GLConfiguration glConf;
    // for anything >= 3.20 Magnum creates a core context
    // if we don't set a version, we get the highest version possible, but a compatibility context :(
    // forward-compatible removes anything deprecated
    glConf.setVersion(GL::Version::GL320);
    glConf.addFlags(GLConfiguration::Flag::ForwardCompatible);
#ifdef CORRADE_IS_DEBUG_BUILD
    glConf.addFlags(GLConfiguration::Flag::Debug);
#endif
    create(conf, glConf);

#ifndef CORRADE_IS_DEBUG_BUILD
    setSwapInterval(0); // disable v-sync
#endif

    CORRADE_ASSERT(GL::Context::current().isCoreProfile(), "OpenGL core context expected", );
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL::Version::GL320);

    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::explicit_attrib_location); // core in 3.3
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::sample_shading);           // core in 4.0
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::texture_multisample);      // core in 3.2

    GL::Renderer::enable(GL::Renderer::Feature::Multisampling);

    // Debug output

    if(logFile.good())
    {
        _debug = Corrade::Containers::pointer<Utility::Debug>(&logFile, Utility::Debug::Flag::NoSpace);
        _warning = Corrade::Containers::pointer<Utility::Warning>(&logFile, Utility::Debug::Flag::NoSpace);
        _error = Corrade::Containers::pointer<Utility::Error>(&logFile, Utility::Debug::Flag::NoSpace);
    }

    profiler.setup(DebugTools::GLFrameProfiler::Value::FrameTime | DebugTools::GLFrameProfiler::Value::GpuDuration, 60);

#ifdef CORRADE_IS_DEBUG_BUILD
    GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
    // redirect debug messages to Corrade Debug output
    GL::DebugOutput::setDefaultCallback();
    // disable unimportant output
    // markers and groups are only used for RenderDoc
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::Marker, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PushGroup, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PopGroup, false);
    // Nvidia drivers: "Buffer detailed info"
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, { 131185 }, false);
#endif

    // Command line

    Utility::Arguments parser;
    parser
        .addOption("mesh", "resources/models/Suzanne.glb")
        //.addOption("mesh", "resources/models/Avocado/Avocado.gltf")
        .setHelp("mesh", "mesh to load")
        .addOption("font", "resources/fonts/Roboto-Regular.ttf")
        .setHelp("font", "GUI font to load")
        .addSkippedPrefix("magnum", "engine-specific options") // ignore --magnum- options
        .setGlobalHelp("Checkered rendering experiment.");
    parser.parse(arguments.argc, arguments.argv);

    // UI

    ImGui::StyleColorsDark();

    std::string fontFile = parser.value<std::string>("font");
    setFont(fontFile.c_str(), 15.0f);

    // Scene

    lightPositions = Containers::array<Vector3>({ { -3.0f, 10.0f, 10.0f } });
    lightColors = Containers::array<Color4>({ 0xffffff_rgbf });
    CORRADE_INTERNAL_ASSERT(lightPositions.size() == lightColors.size());

    cameraObject.setParent(&scene).translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));
    camera->setViewport(framebufferSize());
    updateProjectionMatrix(*camera);

    SceneGraph::Animable3D* animable = new Animable3D(
        cameraObject, Vector3::xAxis(), 3.0f, -5.5f); //new Animable3D(cameraObject, Vector3::yAxis(), 1.5f, 1.0f);
    cameraAnimables.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    manipulator.setParent(&scene);

    // Objects

    Object3D& original = manipulator.addChild<Object3D>();
    std::string sceneFile = parser.value<std::string>("mesh");
    bool loaded = loadScene(sceneFile.c_str(), original);
    CORRADE_ASSERT(loaded, "Failed to load scene", );

    Vector3 center(float(objectGridSize - 1) / 2.0f);

    for(size_t i = 0; i < objectGridSize; i++)
    {
        for(size_t j = 0; j < objectGridSize; j++)
        {
            for(size_t k = 0; k < objectGridSize; k++)
            {
                Object3D& duplicate = duplicateObject(original, *original.parent());
                duplicate.scale(Vector3(10.0f));
                duplicate.translate((Vector3(i, j, -float(k)) - center) * 4.0f);

                for(ColoredDrawable3D* drawable : featuresInChildren<ColoredDrawable3D>(duplicate))
                {
                    drawable->setColor(Color4(i, j, k) * 1.0f / objectGridSize);
                    VelocityDrawable3D* velocityDrawable = new VelocityDrawable3D(
                        static_cast<Object3D&>(drawable->object()), velocityShader, drawable->getMesh());
                    velocityDrawables.add(*velocityDrawable);
                }

                SceneGraph::Animable3D* duplicatedAnimable = new Animable3D(duplicate, Vector3::xAxis(), 3.0f, 5.5f);
                meshAnimables.add(*duplicatedAnimable);
                duplicatedAnimable->setState(SceneGraph::AnimationState::Running);
            }
        }
    }

    // remove original object that was copied in the grid
    for(TexturedDrawable3D* drawable : featuresInChildren<TexturedDrawable3D>(original))
    {
        drawables.remove(*drawable);
    }

    // Framebuffers

    resizeFramebuffers(framebufferSize());

    setSamplePositions();

    // Shaders

    velocityShader = VelocityShader();

    {
        // patch the built-in Phong shader to use sample interpolation
        MagnumShadersSampleInterpolationOverride shaderOverride({ "Phong.vert", "Phong.frag" });

        coloredMaterialShader = Shaders::Phong(Shaders::Phong::Flag(0), lightPositions.size());

        texturedMaterialShader =
            Shaders::Phong(Shaders::Phong::Flag::DiffuseTexture | Shaders::Phong::Flag::SpecularTexture |
                               Shaders::Phong::Flag::NormalTexture,
                           lightPositions.size());
    }

    coloredMaterialShader.setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0xffffff_rgbf)
        .setShininess(80.0f)
        .setLightPositions(lightPositions)
        .setLightColors(lightColors);
    coloredMaterialShader.setLabel("Material shader (colored)");

    texturedMaterialShader.setAmbientColor(0x111111_rgbf)
        .setSpecularColor(0xffffff_rgbf)
        .setShininess(80.0f)
        .setLightPositions(lightPositions)
        .setLightColors(lightColors);
    texturedMaterialShader.setLabel("Material shader (textured)");

    depthBlitShader = DepthBlitShader();
    depthBlitShader.setLabel("Depth blit shader");

    reconstructionShader = ReconstructionShader();
    reconstructionShader.setLabel("Checkerboard resolve shader");

    timeline.start();
}

void Mosaiikki::updateProjectionMatrix(Magnum::SceneGraph::Camera3D& cam)
{
    //constexpr Rad hFOV_4by3 = 90.0_degf;
    //Rad vFOV = Math::atan(Math::tan(hFOV_4by3 * 0.5f) / (4.0f / 3.0f)) * 2.0f;
    constexpr Rad vFOV = 73.74_degf;

    float aspectRatio = Vector2(cam.viewport()).aspectRatio();
    Rad hFOV = Math::atan(Math::tan(vFOV * 0.5f) * aspectRatio) * 2.0f;
    cam.setProjectionMatrix(Matrix4::perspectiveProjection(hFOV, aspectRatio, cameraNear, cameraFar));
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
        framebuffers[i].setLabel(Utility::formatString("Framebuffer {} (quarter-res)", i + 1));

        CORRADE_INTERNAL_ASSERT(framebuffers[i].checkStatus(GL::FramebufferTarget::Read) ==
                                GL::Framebuffer::Status::Complete);
        CORRADE_INTERNAL_ASSERT(framebuffers[i].checkStatus(GL::FramebufferTarget::Draw) ==
                                GL::Framebuffer::Status::Complete);
    }

    outputColorAttachment = GL::Texture2D();
    outputColorAttachment.setStorage(1, GL::TextureFormat::RGBA8, size);
    // filter and wrapping for zoomed GUI debug output
    outputColorAttachment.setMagnificationFilter(SamplerFilter::Nearest);
    outputColorAttachment.setWrapping({ GL::SamplerWrapping::ClampToBorder, GL::SamplerWrapping::ClampToBorder });
    outputColorAttachment.setBorderColor(0x000000_rgbf);
    outputColorAttachment.setLabel("Output color texture");

    outputFramebuffer = GL::Framebuffer({ { 0, 0 }, size });
    outputFramebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), outputColorAttachment, 0);

    CORRADE_INTERNAL_ASSERT(outputFramebuffer.checkStatus(GL::FramebufferTarget::Read) ==
                            GL::Framebuffer::Status::Complete);
    CORRADE_INTERNAL_ASSERT(outputFramebuffer.checkStatus(GL::FramebufferTarget::Draw) ==
                            GL::Framebuffer::Status::Complete);
}

void Mosaiikki::setSamplePositions()
{
    // ARB extension is really only supported by Nvidia (Maxwell and later) and requires GL 4.5
    bool ext_arb = GL::Context::current().isExtensionSupported<GL::Extensions::ARB::sample_locations>();
    bool ext_nv = GL::Context::current().isExtensionSupported<GL::Extensions::NV::sample_locations>();
    bool ext_amd = GL::Context::current().isExtensionSupported<GL::Extensions::AMD::sample_positions>();
    // Haven't found an Intel extension although D3D12 support for it exists

    CORRADE_ASSERT(ext_arb || ext_nv || ext_amd, "No extension for setting sample positions found", );

    GL::Renderer::enable(GL::Renderer::Feature::SampleShading);

    framebuffers[0].bind();

    // set explicit sample locations
    // OpenGL does not specify them, so we have to do it manually using one of the three extensions

    const GLsizei SAMPLE_COUNT = 2;
    Vector2 samplePositions[SAMPLE_COUNT] = { { 0.75f, 0.75f }, { 0.25f, 0.25f } };

    if(ext_arb)
    {
        glFramebufferSampleLocationsfvARB(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, samplePositions[0].data());
    }
    else if(ext_nv)
    {
        glFramebufferSampleLocationsfvNV(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, samplePositions[0].data());
    }
    else if(ext_amd)
    {
        for(GLuint i = 0; i < SAMPLE_COUNT; i++)
        {
            glSetMultisamplefvAMD(GL_SAMPLE_POSITION, i, samplePositions[i].data());
        }
    }

    // read back and report actual sample locations (could be quantized)
    // just a sanity check, really
    for(GLuint i = 0; i < SAMPLE_COUNT; i++)
    {
        // GL_SAMPLE_POSITION reads the default sample location with ARB/NV_sample_locations
        GLenum name = ext_arb ? GL_PROGRAMMABLE_SAMPLE_LOCATION_ARB
                              : (ext_nv ? GL_PROGRAMMABLE_SAMPLE_LOCATION_NV : GL_SAMPLE_POSITION);
        glGetMultisamplefv(name, i, samplePositions[i].data());
    }

    Debug() << "MSAA 2x sample positions:";
    for(Vector2 pos : samplePositions)
    {
        Debug() << pos;
    }

    GL::Renderer::disable(GL::Renderer::Feature::SampleShading);

    GL::defaultFramebuffer.bind();
}

void Mosaiikki::drawEvent()
{
    profiler.beginFrame();

    if(!paused || advanceOneFrame)
    {
        advanceOneFrame = false;

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

            if(meshAnimables.runningCount() > 0)
            {
                // use current frame's jitter
                // this only matters because we blit the velocity depth buffer to reuse it for the quarter resolution pass
                // without it, you can use either jittered or unjittered, as long as they match
                velocityShader.setProjection(matrices[currentFrame]).setOldProjection(oldMatrices[currentFrame]);

                camera->draw(velocityDrawables);
            }
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

            // jitter camera if necessary
            camera->setProjectionMatrix(matrices[currentFrame]);
            camera->draw(drawables);

            GL::Renderer::disable(GL::Renderer::Feature::SampleShading);
        }

        // undo any jitter
        camera->setProjectionMatrix(unjitteredProjection);

        // combine framebuffers

        {
            GL::DebugGroup group(GL::DebugGroup::Source::Application, 1, "Checkerboard resolve");

            outputFramebuffer.bind();

            GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

            reconstructionShader.bindColor(colorAttachments)
                .bindDepth(depthAttachments)
                .bindVelocity(velocityAttachment)
                .setCurrentFrame(currentFrame)
                .setCameraInfo(*camera, cameraNear, cameraFar)
                .setOptions(options.reconstruction);
            reconstructionShader.draw();
        }

        // housekeeping

        currentFrame = (currentFrame + 1) % FRAMES;
        oldMatrices = matrices;
    }

    GL::Framebuffer::blit(
        outputFramebuffer, GL::defaultFramebuffer, GL::defaultFramebuffer.viewport(), GL::FramebufferBlit::Color);

    // render UI

    GL::defaultFramebuffer.bind();

    {
        GL::DebugGroup group(GL::DebugGroup::Source::Application, 2, "imgui");

        ImGuiApplication::drawEvent();
    }

    timeline.nextFrame();
    profiler.endFrame();

    swapBuffers();
    redraw();
}

void Mosaiikki::viewportEvent(ViewportEvent& event)
{
    ImGuiApplication::viewportEvent(event);

    resizeFramebuffers(event.framebufferSize());
    camera->setViewport(event.framebufferSize());
    updateProjectionMatrix(*camera);
}

void Mosaiikki::keyReleaseEvent(KeyEvent& event)
{
    ImGuiApplication::keyReleaseEvent(event);
    if(event.isAccepted())
        return;

    if(event.modifiers())
        return;

    switch(event.key())
    {
        case KeyEvent::Key::Space:
            paused = !paused;
            break;
        case KeyEvent::Key::Right:
            advanceOneFrame = true;
            break;
        // can't use Tab because it's eaten by imgui's keyboard navigation
        //case KeyEvent::Key::Tab:
        case KeyEvent::Key::H:
            hideUI = !hideUI;
            break;
        default:
            return;
    }

    event.setAccepted();
}

void Mosaiikki::buildUI()
{
    if(hideUI)
        return;

    //ImGui::ShowDemoWindow();

    const ImVec2 margin = { 5.0f, 5.0f };

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

        {
            ImGuiDisabledZone zone(options.reconstruction.assumeOcclusion);
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 2.0f); // slider size (without label)
            ImGui::SliderFloat("Depth tolerance", &options.reconstruction.depthTolerance, 0.0f, 0.5f, "%.3f");
            if(ImGui::IsItemHovered())
                ImGui::SetTooltip("Maximum allowed view space depth difference before assuming occlusion");
        }

#ifdef CORRADE_IS_DEBUG_BUILD

        ImGui::Separator();

        static const char* const debugSamplesOptions[] = { "Combined", "Even", "Odd (jittered)" };
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
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
                ImGui::ColorButton("magenta", ImVec4(Color4::magenta()));
                ImGui::SameLine();
                ImGui::Text("Old pixel was occluded");
            }
            {
                ImGui::ColorButton("yellow", ImVec4(Color4::yellow()));
                ImGui::SameLine();
                ImGui::Text("Old pixel position is outside the screen");
            }
            {
                ImGui::ColorButton("cyan", ImVec4(Color4::cyan()));
                ImGui::SameLine();
                ImGui::Text("Old pixel information is missing (jitter was cancelled out)");
            }
            ImGui::EndTooltip();
        }

#endif

        ImGui::Separator();

        if(ImGui::CollapsingHeader("Controls"))
        {
            ImGui::Text("Space");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.45f);
            ImGui::Text("Pause/Continue");

            ImGui::Text("Right Arrow");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.45f);
            ImGui::Text("Next frame");

            ImGui::Text("Right Mouse");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.45f);
            ImGui::Text("Zoom");

            ImGui::Text("H");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.45f);
            ImGui::Text("Hide UI");
        }

        const ImVec2 pos = { ImGui::GetIO().DisplaySize.x - ImGui::GetWindowSize().x - margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    }
    ImGui::End();

    ImGui::Begin(
        "Stats", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::Text("%s", profiler.statistics().c_str());
        if(paused)
            ImGui::TextColored(ImVec4(Color4::yellow()), "PAUSED");

        const ImVec2 pos = { margin.x, margin.y };
        ImGui::SetWindowPos(pos);
    }
    ImGui::End();

    if(!ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        ImGui::Begin("Zoom", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        {
            static const float zoom = 8.0f;
            static const Vector2 imageSize = { 256.0f, 256.0f };

            const Vector2 screenSize = Vector2(ImGui::GetIO().DisplaySize);
            Vector2 uv = (Vector2(ImGui::GetMousePos()) + Vector2(0.5f)) / screenSize;
            uv.y() = 1.0f - uv.y();
            const Range2D range = Range2D::fromCenter(uv, imageSize / screenSize / zoom * 0.5f);
            ImGuiIntegration::image(outputColorAttachment, imageSize, range);

            ImGui::SetMouseCursor(ImGuiMouseCursor_None);

            Vector2 pos = Vector2(ImGui::GetMousePos()) - (Vector2(ImGui::GetWindowSize()) * 0.5f);
            ImGui::SetWindowPos(ImVec2(pos));
        }
        ImGui::End();
    }

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

    Containers::arrayResize(meshes, meshes.size() + importer->meshCount());
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

    // load materials

    Containers::arrayResize(materials, materials.size() + importer->materialCount());
    for(UnsignedInt i = 0; i < importer->materialCount(); i++)
    {
        Containers::Pointer<Trade::AbstractMaterialData> data = importer->material(i);
        if(data && data->type() == Trade::MaterialType::Phong)
        {
            materials[i] = std::move(static_cast<Trade::PhongMaterialData&>(*data));
        }
    }

    // load textures

    Magnum::UnsignedInt textureOffset = textures.size();

    Containers::arrayResize(textures, textures.size() + importer->textureCount());
    for(UnsignedInt i = 0; i < importer->textureCount(); i++)
    {
        Containers::Optional<Trade::TextureData> textureData = importer->texture(i);
        if(textureData && textureData->type() == Trade::TextureData::Type::Texture2D)
        {
            Containers::Optional<Trade::ImageData2D> imageData = importer->image2D(textureData->image(), 0 /* level */);
            if(imageData)
            {
                GL::TextureFormat format;
                switch(imageData->format())
                {
                    case PixelFormat::RGB8Unorm:
                        format = GL::TextureFormat::RGB8;
                        break;
                    case PixelFormat::RGBA8Unorm:
                        format = GL::TextureFormat::RGB8;
                        break;
                    default:
                        continue;
                }
                GL::Texture2D texture;
                texture
                    // lod calculation is something roughly equivalent log2(max(len(dFdx(uv)), len(dFdy(uv)))
                    // halving the rendering resolution doubles the derivate length
                    // so offset lod to lower mip level for full resolution (log2(sqrt(2)) = 0.5)
                    .setLodBias(-0.5f)
                    .setMagnificationFilter(textureData->magnificationFilter())
                    .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
                    .setWrapping(textureData->wrapping().xy())
                    .setStorage(Math::log2(imageData->size().max()) + 1, format, imageData->size())
                    .setSubImage(0, {}, *imageData)
                    .generateMipmap();
                textures[i] = std::move(texture);
            }
        }
    }

    // add objects recursively

    for(UnsignedInt objectId : sceneData->children3D())
    {
        addObject(*importer, objectId, parent, textureOffset);
    }

    return true;
}

void Mosaiikki::addObject(Trade::AbstractImporter& importer,
                          UnsignedInt objectId,
                          Object3D& parent,
                          Magnum::UnsignedInt textureOffset)
{
    // meshes are compiled and accessible at this point

    Containers::Pointer<Trade::ObjectData3D> objectData = importer.object3D(objectId);
    if(objectData)
    {
        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 &&
           meshes[objectData->instance()])
        {
            bool textured = false;
            const Int materialId = static_cast<Trade::MeshObjectData3D*>(objectData.get())->material();
            if(materialId != -1 && materials[materialId])
            {
                const Trade::PhongMaterialData& material = *materials[materialId];
                constexpr Trade::PhongMaterialData::Flags texturedFlags =
                    Trade::PhongMaterialData::Flag::DiffuseTexture | Trade::PhongMaterialData::Flag::SpecularTexture |
                    Trade::PhongMaterialData::Flag::NormalTexture;

                if((material.flags() & texturedFlags) == texturedFlags)
                {
                    SceneGraph::Drawable3D* drawable = new TexturedDrawable3D(object,
                                                                              texturedMaterialShader,
                                                                              *meshes[objectData->instance()],
                                                                              textures,
                                                                              textureOffset,
                                                                              material);
                    drawables.add(*drawable);
                    textured = true;
                }
            }

            if(!textured)
            {
                SceneGraph::Drawable3D* drawable =
                    new ColoredDrawable3D(object, coloredMaterialShader, *meshes[objectData->instance()]);
                drawables.add(*drawable);
            }
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object, textureOffset);
        }
    }
}

Mosaiikki::Object3D& Mosaiikki::duplicateObject(Object3D& object, Object3D& parent)
{
    CORRADE_INTERNAL_ASSERT(!object.isScene());

    Object3D& duplicate = parent.addChild<Object3D>();
    duplicate.setTransformation(object.transformation());

    TexturedDrawable3D* texturedDrawable = feature<TexturedDrawable3D>(object);
    if(texturedDrawable)
    {
        TexturedDrawable3D* newDrawable = new TexturedDrawable3D(*texturedDrawable, duplicate);
        drawables.add(*newDrawable);
    }

    ColoredDrawable3D* coloredDrawable = feature<ColoredDrawable3D>(object);
    if(coloredDrawable)
    {
        ColoredDrawable3D* newDrawable = new ColoredDrawable3D(*coloredDrawable, duplicate);
        drawables.add(*newDrawable);
    }

    for(Object3D& child : object.children())
    {
        duplicateObject(child, duplicate);
    }

    return duplicate;
}
