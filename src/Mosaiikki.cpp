#include "Mosaiikki.h"

#include "Scene.h"
#include "Feature.h"
#include "MagnumShadersSampleInterpolationOverride.h"
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/FormatStl.h>

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
    velocityFramebuffer(NoCreate),
    velocityAttachment(NoCreate),
    velocityDepthAttachment(NoCreate),
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

    // TODO remove
    // ASAN test
    /*
    char* test = new char[2];
    test[1] = 0;
    test[2] = 0;

    Debug() << test;
    */

    // Command line

    Utility::Arguments parser;
    parser.addOption("font", "resources/fonts/Roboto-Regular.ttf")
        .setHelp("font", "GUI font to load")
        .addSkippedPrefix("magnum", "engine-specific options") // ignore --magnum- options
        .setGlobalHelp("Checkered rendering experiment.");
    parser.parse(arguments.argc, arguments.argv);

    // UI

    ImGui::StyleColorsDark();

    std::string fontFile = parser.value<std::string>("font");
    setFont(fontFile.c_str(), 15.0f);

    // Framebuffers

    resizeFramebuffers(framebufferSize());

    setSamplePositions();

    // Shaders

    depthBlitShader = DepthBlitShader();
    depthBlitShader.setLabel("Depth blit shader");

    ReconstructionShader::Flags reconstructionFlags;
#ifdef CORRADE_IS_DEBUG_BUILD
    reconstructionFlags |= ReconstructionShader::Flag::Debug;
#endif
    reconstructionShader = ReconstructionShader(reconstructionFlags);
    reconstructionShader.setLabel("Checkerboard resolve shader");

    // Scene

    {
        // patch the built-in Phong shader to use sample interpolation
        MagnumShadersSampleInterpolationOverride shaderOverride({ "Phong.vert", "Phong.frag" });

        scene.emplace();
    }

    for(Containers::Optional<GL::Texture2D>& texture : scene->textures)
    {
        if(texture)
        {
            // LOD calculation is something roughly equivalent to: log2(max(len(dFdx(uv)), len(dFdy(uv)))
            // halving the rendering width/height doubles the derivate length
            // after upsampling, textures would become blurry compared to full-resolution rendering
            // so offset LOD to lower mip level to full resolution equivalent (log2(sqrt(2)) = 0.5)
            texture->setLodBias(-0.5f);
        }
    }

    scene->camera->setViewport(framebufferSize());
    updateProjectionMatrix(*scene->camera);

    timeline.start();
}

void Mosaiikki::updateProjectionMatrix(Magnum::SceneGraph::Camera3D& cam)
{
    //constexpr Rad hFOV_4by3 = 90.0_degf;
    //Rad vFOV = Math::atan(Math::tan(hFOV_4by3 * 0.5f) / (4.0f / 3.0f)) * 2.0f;
    constexpr Rad vFOV = 73.74_degf;

    float aspectRatio = Vector2(cam.viewport()).aspectRatio();
    Rad hFOV = Math::atan(Math::tan(vFOV * 0.5f) * aspectRatio) * 2.0f;
    cam.setProjectionMatrix(Matrix4::perspectiveProjection(hFOV, aspectRatio, scene->cameraNear, scene->cameraFar));
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
    velocityFramebuffer.mapForDraw({ { VelocityShader::VelocityOutput, GL::Framebuffer::ColorAttachment(0) } });
    velocityFramebuffer.setLabel("Velocity framebuffer");

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
        framebuffers[i].mapForDraw({ { Shaders::Generic3D::ColorOutput, GL::Framebuffer::ColorAttachment(0) } });
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
    // no depth buffer needed
    outputFramebuffer.mapForDraw({ { ReconstructionShader::ColorOutput, GL::Framebuffer::ColorAttachment(0) } });
    outputFramebuffer.setLabel("Output framebuffer");

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

        scene->meshAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());
        scene->cameraAnimables.step(timeline.previousFrameTime(), timeline.previousFrameDuration());

        constexpr GL::Renderer::DepthFunction depthFunction = GL::Renderer::DepthFunction::LessOrEqual; // default: Less

        GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
        GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
        GL::Renderer::setDepthFunction(depthFunction);

        const Matrix4 unjitteredProjection = scene->camera->projectionMatrix();

        static Array<FRAMES, Matrix4> oldMatrices = { Matrix4(Math::IdentityInit), Matrix4(Math::IdentityInit) };
        Array<FRAMES, Matrix4> matrices;
        // jitter viewport half a pixel to the right = one pixel in the full-res framebuffer
        // = width of NDC divided by full-res pixel count
        const float offset = 2.0f / scene->camera->viewport().x();
        matrices[JITTERED_FRAME] = Matrix4::translation(Vector3::xAxis(offset)) * unjitteredProjection;
        matrices[1 - JITTERED_FRAME] = unjitteredProjection;

        // fill velocity buffer

        if(options.reconstruction.createVelocityBuffer)
        {
            GL::DebugGroup group(GL::DebugGroup::Source::Application, 0, "Velocity buffer");

            velocityFramebuffer.bind();
            velocityFramebuffer.clearColor(0, 0_rgbf);
            velocityFramebuffer.clearDepth(1.0f);

            if(scene->meshAnimables.runningCount() > 0)
            {
                // use current frame's jitter
                // this only matters because we blit the velocity depth buffer to reuse it for the quarter resolution pass
                // without it, you can use either jittered or unjittered, as long as they match
                scene->velocityShader.setProjectionMatrix(matrices[currentFrame])
                    .setOldProjectionMatrix(oldMatrices[currentFrame]);

                scene->camera->draw(scene->velocityDrawables);
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
            scene->camera->setProjectionMatrix(matrices[currentFrame]);
            scene->camera->draw(scene->drawables);

            GL::Renderer::disable(GL::Renderer::Feature::SampleShading);
        }

        // undo any jitter
        scene->camera->setProjectionMatrix(unjitteredProjection);

        // combine framebuffers

        {
            GL::DebugGroup group(GL::DebugGroup::Source::Application, 1, "Checkerboard resolve");

            outputFramebuffer.bind();

            GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

            reconstructionShader.bindColor(colorAttachments)
                .bindDepth(depthAttachments)
                .bindVelocity(velocityAttachment)
                .setCurrentFrame(currentFrame)
                .setCameraInfo(*scene->camera, scene->cameraNear, scene->cameraFar)
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
    scene->camera->setViewport(event.framebufferSize());
    updateProjectionMatrix(*scene->camera);
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
                     reinterpret_cast<int*>(&options.reconstruction.debug.showSamples),
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

    for(size_t i = 0; i < scene->meshAnimables.size(); i++)
    {
        scene->meshAnimables[i].setState(options.scene.animatedObjects ? SceneGraph::AnimationState::Running
                                                                       : SceneGraph::AnimationState::Paused);
    }

    // TODO rotation (e.g. panning) instead of translation animation
    if(scene->cameraAnimables.size() > 0)
        scene->cameraAnimables[0].setState(options.scene.animatedCamera ? SceneGraph::AnimationState::Running
                                                                        : SceneGraph::AnimationState::Paused);
}
