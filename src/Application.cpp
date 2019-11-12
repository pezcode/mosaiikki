#include "Application.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/RenderbufferFormat.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Containers/ArrayView.h>

namespace GL = Magnum::GL;
using namespace Corrade;

Application::Application(const Arguments& arguments) :
    ImGuiApplication(arguments),
    framebuffers {
        GL::Framebuffer(Magnum::NoCreate),
        GL::Framebuffer(Magnum::NoCreate)
    },
    currentFramebuffer(0)
{
    setWindowTitle("calculi");
    // TODO how to set size without passing Configuration to constructor?

    Utility::Arguments parser;
    parser.parse(arguments.argc, arguments.argv);

    // UI

    const char* fontName = "Roboto-Regular.ttf";
    const Containers::ArrayView<const char> font = Utility::Resource("font").getRaw(fontName);
    setFont(font.data(), font.size(), 15.0f);

    // Framebuffers

    const Magnum::Vector2i size = framebufferSize();
    CORRADE_INTERNAL_ASSERT(size % 2 == Magnum::Vector2i(0, 0));

    for (size_t i = 0; i < 2; i++)
    {
        framebuffers[i] = GL::Framebuffer({ { 0, 0 }, size / 2 });
        colorAttachments[i].setStorage(1, GL::TextureFormat::RGBA8, size);
        depthStencilAttachments[i].setStorage(GL::RenderbufferFormat::Depth24Stencil8, size);
        framebuffers[i].attachTexture(GL::Framebuffer::ColorAttachment(0), colorAttachments[i], 0);
        framebuffers[i].attachRenderbuffer(GL::Framebuffer::BufferAttachment::DepthStencil, depthStencilAttachments[i]);
    }
}

void Application::drawEvent()
{
    // Ubuntu purple (Mid aubergine)
    // https://design.ubuntu.com/brand/colour-palette/
    const Magnum::Color3 clearColor = Magnum::Color3(0x5E, 0x27, 0x50) / 255.0f;
    GL::Renderer::setClearColor(clearColor);

    GL::Framebuffer& framebuffer = framebuffers[currentFramebuffer];

    framebuffer.bind();
    framebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    // blit with interpolation
    GL::Framebuffer::blit(framebuffer, GL::defaultFramebuffer, GL::defaultFramebuffer.viewport(), GL::FramebufferBlit::Color);

    currentFramebuffer = (currentFramebuffer + 1) % 2;

    GL::defaultFramebuffer.bind();

    ImGuiApplication::drawEvent();
}

void Application::buildUI()
{
    //ImGui::ShowTestWindow();
    ImGui::Text("Checkerboard rendering");
}
