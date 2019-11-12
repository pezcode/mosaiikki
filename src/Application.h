#pragma once

#include "ImGuiApplication.h"
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>

class Application : public ImGuiApplication
{
public:
    explicit Application(const Arguments& arguments);

private:
    virtual void drawEvent() override;
    virtual void buildUI() override;

    // checkerboard framebuffers
    // quarter size (half width, half height)
    Magnum::GL::Framebuffer framebuffers[2];
    Magnum::GL::Texture2D colorAttachments[2];
    // we could probably reuse the depth attachment
    Magnum::GL::Renderbuffer depthStencilAttachments[2];
    size_t currentFramebuffer;
};
