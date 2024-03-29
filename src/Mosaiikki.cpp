#include "Mosaiikki.h"

#include "Scene.h"
#include "Feature.h"
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/MeshTools/FullScreenTriangle.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Format.h>

using namespace Magnum;
using namespace Corrade;
using namespace Magnum::Math::Literals;
using namespace Feature;

const char* Mosaiikki::NAME = "mosaiikki";

Mosaiikki::Mosaiikki(const Arguments& arguments) :
    ImGuiApplication(arguments, NoCreate),
    logFile(std::string(NAME) + ".log", std::fstream::out | std::fstream::trunc),
    fullscreenTriangle(NoCreate),
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
    // Redirect log to file

    if(logFile.good())
    {
        _debug = Corrade::Containers::pointer<Utility::Debug>(&logFile, Utility::Debug::Flag::NoSpace);
        _warning = Corrade::Containers::pointer<Utility::Warning>(&logFile, Utility::Debug::Flag::NoSpace);
        _error = Corrade::Containers::pointer<Utility::Error>(&logFile, Utility::Debug::Flag::NoSpace);
    }

    // Configuration and GL context

    Configuration conf;
    conf.setSize({ 800, 600 });
    conf.setTitle(NAME);
    conf.setWindowFlags(Configuration::WindowFlag::Resizable);

    constexpr GL::Version GLVersion = GL::Version::GL320;

    GLConfiguration glConf;
    // for anything >= 3.20 Magnum creates a core context
    // if we don't set a version on Nvidia drivers, we get the highest version possible, but a compatibility context :(
    // forward-compatible removes anything deprecated
    glConf.setVersion(GLVersion);
    glConf.addFlags(GLConfiguration::Flag::ForwardCompatible);
#ifdef CORRADE_IS_DEBUG_BUILD
    glConf.addFlags(GLConfiguration::Flag::Debug);
#endif
    create(conf, glConf);

#ifndef CORRADE_IS_DEBUG_BUILD
    setSwapInterval(0); // disable v-sync
#endif

    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::explicit_attrib_location); // core in 3.3
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::sample_shading);           // core in 4.0
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::texture_multisample);      // core in 3.2
    MAGNUM_ASSERT_GL_EXTENSION_SUPPORTED(GL::Extensions::ARB::uniform_buffer_object);    // core in 3.1

    GL::Renderer::enable(GL::Renderer::Feature::Multisampling);

    // Debug output

    profiler.setup(DebugTools::FrameProfilerGL::Value::FrameTime | DebugTools::FrameProfilerGL::Value::GpuDuration, 60);

#ifdef CORRADE_IS_DEBUG_BUILD
    GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
    // output debug messages on the same thread as the GL commands
    // this fixes issues with Corrade::Debug having per-thread output streams
    GL::Renderer::enable(GL::Renderer::Feature::DebugOutputSynchronous);
    // redirect debug messages to Corrade::Debug
    GL::DebugOutput::setDefaultCallback();
    // disable unimportant output
    // markers and groups are only used for RenderDoc
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::Marker, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PushGroup, false);
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Application, GL::DebugOutput::Type::PopGroup, false);
    // Nvidia drivers: "Buffer detailed info"
    GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, { 131185 }, false);
#endif

    // UI

    ImGui::StyleColorsDark();

    Utility::Resource rs("resources");
    Containers::ArrayView<const char> font = rs.getRaw("fonts/Roboto-Regular.ttf");
    setFont(font.data(), font.size(), 15.0f);

    // Framebuffers

    resizeFramebuffers(framebufferSize());

    setSamplePositions();

    // Shaders

    depthBlitShader = DepthBlitShader();
    depthBlitShader.setLabel("Depth blit shader");

    ReconstructionShader::Flags reconstructionFlags = ReconstructionShader::Flags()
#ifdef CORRADE_IS_DEBUG_BUILD
                                                      | ReconstructionShader::Flag::Debug
#endif
        ;
    reconstructionShader = ReconstructionShader(reconstructionFlags);
    reconstructionShader.setLabel("Checkerboard resolve shader");

    // Scene

    scene.emplace();

    for(Containers::Pointer<GL::Texture2D>& texture : scene->textures)
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

    CORRADE_INTERNAL_CONSTEXPR_ASSERT(GLVersion >= GL::Version::GL300);
    fullscreenTriangle = MeshTools::fullScreenTriangle(GLVersion);

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
    velocityFramebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), velocityAttachment, 0 /* level */);
    velocityFramebuffer.attachTexture(GL::Framebuffer::BufferAttachment::Depth, velocityDepthAttachment, 0 /* level */);
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
        framebuffers[i].attachTextureLayer(GL::Framebuffer::ColorAttachment(0), colorAttachments, i /* layer */);
        framebuffers[i].attachTextureLayer(GL::Framebuffer::BufferAttachment::Depth, depthAttachments, i /* layer */);
        framebuffers[i].mapForDraw({ { Shaders::GenericGL3D::ColorOutput, GL::Framebuffer::ColorAttachment(0) } });
        framebuffers[i].setLabel(Utility::format("Framebuffer {} (quarter-res)", i + 1));

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
    outputFramebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), outputColorAttachment, 0 /* level */);
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
    const GLsizei SAMPLE_COUNT = 2;
    const Vector2 samplePositions[SAMPLE_COUNT] = { { 0.75f, 0.75f }, { 0.25f, 0.25f } };

    // set explicit MSAA sample locations
    // OpenGL does not specify them, so we have to do it manually using one of three extensions

    // ARB extension is really only supported by Nvidia (Maxwell and later) and requires GL 4.5
    bool ext_arb = GL::Context::current().isExtensionSupported<GL::Extensions::ARB::sample_locations>();
    bool ext_nv = GL::Context::current().isExtensionSupported<GL::Extensions::NV::sample_locations>();
    bool ext_amd = GL::Context::current().isExtensionSupported<GL::Extensions::AMD::sample_positions>();
    // Haven't found an Intel extension although D3D12 support for it exists

    if(!(ext_arb || ext_nv || ext_amd))
    {
        // none of the extensions are supported (also happens in RenderDoc which force-disables it)
        // warn here instead of aborting because you might have the correct sample positions anyway (which we check below)
        // the sample positions we request seem to be the default on Nvidia GPUs
        Warning() << "No extension for setting sample positions found!";
    }

    for(size_t frame = 0; frame < FRAMES; frame++)
    {
        framebuffers[frame].bind();

        if(ext_arb)
        {
            int supportedSampleCount = 0;
            glGetIntegerv(GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_ARB, &supportedSampleCount);
            CORRADE_INTERNAL_ASSERT(SAMPLE_COUNT <= supportedSampleCount);

            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_ARB, GL_TRUE);
            glFramebufferSampleLocationsfvARB(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, samplePositions[0].data());
        }
        else if(ext_nv)
        {
            int supportedSampleCount = 0;
            glGetIntegerv(GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV, &supportedSampleCount);
            CORRADE_INTERNAL_ASSERT(SAMPLE_COUNT <= supportedSampleCount);

            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV, GL_TRUE);
            glFramebufferSampleLocationsfvNV(GL_FRAMEBUFFER, 0, SAMPLE_COUNT, samplePositions[0].data());
        }
        else if(ext_amd)
        {
            for(GLuint i = 0; i < SAMPLE_COUNT; i++)
            {
                glSetMultisamplefvAMD(GL_SAMPLE_POSITION, i, samplePositions[i].data());
            }
        }
    }

    // read back, report and check actual sample locations

    bool mismatch = false;
    Debug(Debug::Flag::NoSpace) << "MSAA " << SAMPLE_COUNT << "x sample positions:";
    for(GLuint i = 0; i < SAMPLE_COUNT; i++)
    {
        Vector2 position;
        // GL_SAMPLE_POSITION reads the default sample location with ARB/NV_sample_locations
        GLenum name = ext_arb ? GL_PROGRAMMABLE_SAMPLE_LOCATION_ARB
                              : (ext_nv ? GL_PROGRAMMABLE_SAMPLE_LOCATION_NV : GL_SAMPLE_POSITION);
        glGetMultisamplefv(name, i, position.data());
        Debug(Debug::Flag::NoSpace) << i << ": " << position;

        // positions can be quantized, so only do a rough comparison
        // we can query the quantization amount (SUBSAMPLE_DISTANCE_AMD and SAMPLE_LOCATION_SUBPIXEL_BITS_NV)
        // but we're only interested in an acceptable absolute error anyway
        constexpr float allowedError = 1.0f / 8.0f;
        if((Math::abs(position - samplePositions[i]) > Vector2(allowedError)).any())
        {
            mismatch = true;
        }
    }

    if(mismatch)
        Error() << "Wrong sample positions, output will likely be incorrect!";

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

        GL::Renderer::disable(GL::Renderer::Feature::Blending);
        GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
        GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                       GL::Renderer::BlendFunction::OneMinusSourceAlpha);

        static Containers::StaticArray<FRAMES, Matrix4> oldMatrices = { Matrix4(Math::IdentityInit),
                                                                        Matrix4(Math::IdentityInit) };
        Containers::StaticArray<FRAMES, Matrix4> matrices;

        // jitter viewport half a pixel to the right = one pixel in the full-res framebuffer
        // = width of NDC divided by full-res pixel count
        const Matrix4 unjitteredProjection = scene->camera->projectionMatrix();
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

            // dynamic objects only
            // camera velocity for static objects is calculated with reprojection in the checkerboard resolve pass
            if(scene->meshAnimables.runningCount() > 0)
            {
                // offset depth for the depth blit, otherwise the depth test might fail in the quarter-res pass
                // not entirely sure what causes this, could be floating point inaccuracy?
                // slope bias allows an offset based on triangle depth gradient,
                // without it we'd need to use a larger constant bias and pray it works
                GL::Renderer::enable(GL::Renderer::Feature::PolygonOffsetFill);
                GL::Renderer::setPolygonOffset(1 /* slope bias */, 1 /* constant bias */);

                // use current frame's jitter
                // this only matters because we blit the velocity depth buffer to reuse it for the quarter resolution pass
                // without it, you can use either jittered or unjittered, as long as they match
                scene->velocityShader.setProjectionMatrix(matrices[currentFrame])
                    .setOldProjectionMatrix(oldMatrices[currentFrame]);

                scene->camera->draw(scene->velocityDrawables);

                // transparent objects shouldn't write to the depth buffer if we blit and reuse it in the quarter-res scene pass
                // TODO without depth writes they now have to be properly sorted back to front
                // we kinda do this during scene creation, which works because the camera position is static
                // for the opaque velocity drawables, we could sort front to back to reduce overdraw
                // should we do the same for the normal renderables? it'll be a bit annoying to duplicate the
                // Renderables added in loadScene :<
                GL::Renderer::setDepthMask(GL_FALSE);
                scene->camera->draw(scene->transparentVelocityDrawables);
                GL::Renderer::setDepthMask(GL_TRUE);

                GL::Renderer::disable(GL::Renderer::Feature::PolygonOffsetFill);
            }
        }

        // render scene at quarter resolution

        {
            GL::DebugGroup group(GL::DebugGroup::Source::Application, 0, "Scene rendering (quarter-res)");

            GL::Framebuffer& framebuffer = framebuffers[currentFrame];
            framebuffer.bind();

            // run fragment shader for each sample
            GL::Renderer::enable(GL::Renderer::Feature::SampleShading);
            GL::Renderer::setMinSampleShading(1.0f);

            // copy and reuse velocity depth buffer
            if(options.reconstruction.createVelocityBuffer && options.reuseVelocityDepth)
            {
                GL::DebugGroup group2(GL::DebugGroup::Source::Application, 0, "Velocity depth blit");

                GL::Renderer::setDepthFunction(
                    GL::Renderer::DepthFunction::Always); // fullscreen pass, always pass depth test
                GL::Renderer::setColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // disable color writing

                // blit to quarter res with max filter
                depthBlitShader.bindDepth(velocityDepthAttachment);
                depthBlitShader.draw(fullscreenTriangle);

                GL::Renderer::setColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                GL::Renderer::setDepthFunction(depthFunction);

                // implementations can choose to optimize storage by not writing actual depth
                // values and reconstructing them during sampling at the default sample positions
                // these commands force correct per-sample depth to be written to the depth buffer
                if(glEvaluateDepthValuesARB)
                    glEvaluateDepthValuesARB();
                else if(glResolveDepthValuesNV)
                    glResolveDepthValuesNV();
            }
            else
            {
                framebuffer.clearDepth(1.0f);
            }

            const Color4 clearColor = Color4::fromSrgb(0x772953_rgbf); // Ubuntu Canonical aubergine
            framebuffer.clearColor(0, clearColor);

            // use jittered camera if necessary
            scene->camera->setProjectionMatrix(matrices[currentFrame]);

            GL::Renderer::enable(GL::Renderer::Feature::Blending);

            scene->camera->draw(scene->drawables);

            GL::Renderer::disable(GL::Renderer::Feature::Blending);

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
                .setOptions(options.reconstruction)
                .setBuffer();
            reconstructionShader.draw(fullscreenTriangle);
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

        ImGui::BeginDisabled(!options.reconstruction.createVelocityBuffer);
        ImGui::Checkbox("Re-use velocity depth", &options.reuseVelocityDepth);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Downsample and re-use the velocity pass depth buffer for the quarter-res pass");
        ImGui::EndDisabled();

        ImGui::Checkbox("Always assume occlusion", &options.reconstruction.assumeOcclusion);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Always assume an old pixel is occluded in the current frame if the pixel moved by more than a quarter-res pixel.\n"
                "If this is disabled, a more expensive check against the average surrounding depth value is used.");

        ImGui::BeginDisabled(options.reconstruction.assumeOcclusion);
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 2.0f); // slider size (without label)
        ImGui::SliderFloat("Depth tolerance", &options.reconstruction.depthTolerance, 0.0f, 0.5f, "%.3f");
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Maximum allowed view space depth difference before assuming occlusion");
        ImGui::EndDisabled();

        ImGui::Checkbox("Use differential blending", &options.reconstruction.differentialBlending);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "When blending pixel neighbor horizontal/vertical axes, weight their contribution by how small the color difference is.\n"
                "This greatly reduces checkerboard artifacts at sharp edges.");

#ifdef CORRADE_IS_DEBUG_BUILD

        ImGui::Separator();

        static const char* const debugSamplesOptions[] = { "Combined", "Even", "Odd (jittered)" };
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::Combo("Show samples",
                     reinterpret_cast<int*>(&options.reconstruction.debug.showSamples),
                     debugSamplesOptions,
                     Containers::arraySize(debugSamplesOptions));

        ImGui::BeginDisabled(!options.reconstruction.createVelocityBuffer);
        ImGui::Checkbox("Show velocity buffer", &options.reconstruction.debug.showVelocity);
        ImGui::EndDisabled();

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

    for(size_t i = 0; i < scene->cameraAnimables.size(); i++)
    {
        scene->cameraAnimables[0].setState(options.scene.animatedCamera ? SceneGraph::AnimationState::Running
                                                                        : SceneGraph::AnimationState::Paused);
    }
}
