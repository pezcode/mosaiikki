#pragma once

#include "ImGuiApplication.h"
#include "Options.h"
#include "Scene.h"
#include "Shaders/ReconstructionShader.h"
#include "Shaders/DepthBlitShader.h"
#include <Magnum/Timeline.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/DebugTools/FrameProfiler.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Containers/Pointer.h>
#include <fstream>

class Mosaiikki : public ImGuiApplication
{
public:
    explicit Mosaiikki(const Arguments& arguments);

    static const char* NAME;

private:
    virtual void drawEvent() override;
    virtual void viewportEvent(ViewportEvent& event) override;
    virtual void keyReleaseEvent(KeyEvent& event) override;
    virtual void buildUI() override;

    void updateProjectionMatrix(Magnum::SceneGraph::Camera3D& camera);
    void resizeFramebuffers(Magnum::Vector2i frameBufferSize);
    void setSamplePositions();

    // debug output

    std::fstream logFile;
    // global instances to override the ostream for all instances created later
    // we need to override each of them separately
    // don't use these, they're nullptr if the file can't be created
    Corrade::Containers::Pointer<Corrade::Utility::Debug> _debug;
    Corrade::Containers::Pointer<Corrade::Utility::Warning> _warning;
    Corrade::Containers::Pointer<Corrade::Utility::Error> _error;

    Magnum::DebugTools::FrameProfilerGL profiler;

    // scene

    Corrade::Containers::Pointer<Scene> scene;

    Magnum::GL::Mesh fullscreenTriangle;

    Magnum::Timeline timeline;

    bool paused = false;
    bool advanceOneFrame = false;

    bool hideUI = false;

    // checkerboard rendering

    Magnum::GL::Framebuffer velocityFramebuffer;
    Magnum::GL::Texture2D velocityAttachment;
    Magnum::GL::Texture2D velocityDepthAttachment;

    static constexpr size_t FRAMES = 2;
    static constexpr size_t JITTERED_FRAME = 1;
    size_t currentFrame = 0;

    // quarter-size framebuffers (half width, half height)
    Magnum::GL::Framebuffer framebuffers[FRAMES];
    Magnum::GL::MultisampleTexture2DArray colorAttachments;
    Magnum::GL::MultisampleTexture2DArray depthAttachments;

    DepthBlitShader depthBlitShader;

    Magnum::GL::Framebuffer outputFramebuffer;
    Magnum::GL::Texture2D outputColorAttachment;

    ReconstructionShader reconstructionShader;

    Options options;
};
