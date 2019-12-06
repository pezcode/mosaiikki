#include "ImGuiApplication.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

using namespace Magnum;

ImGuiApplication::ImGuiApplication(const Arguments& arguments,
                                   const Configuration& configuration,
                                   const GLConfiguration& glConfiguration) :
    Magnum::Platform::Application(arguments, configuration, glConfiguration)
{
    init();
}

ImGuiApplication::ImGuiApplication(const Arguments& arguments, NoCreateT) :
    Magnum::Platform::Application(arguments, NoCreate)
{
}

ImGuiApplication::~ImGuiApplication()
{
    imgui.release();
    ImGui::DestroyContext();
}

void ImGuiApplication::create(const Configuration& configuration, const GLConfiguration& glConfiguration)
{
    Platform::Application::create(configuration, glConfiguration);
    init();
}

bool ImGuiApplication::tryCreate(const Configuration& configuration, const GLConfiguration& glConfiguration)
{
    if(Platform::Application::tryCreate(configuration, glConfiguration))
    {
        init();
        return true;
    }
    return false;
}

void ImGuiApplication::init()
{
    ImGui::CreateContext();
    // we can't call an overridden method from the base class constructor.
    // any configuration in the derived class must happen after the base
    // class constructor. that means the default font atlas is created
    // and uploaded, and then later created again after setFont.
    // not a massive deal, but one workaround would be creating the
    // implementation instance in the first drawEvent.
    imgui = Magnum::ImGuiIntegration::Context(*ImGui::GetCurrentContext(), uiSize(), windowSize(), framebufferSize());
}

void ImGuiApplication::drawEvent()
{
    imgui.newFrame();

    // enable text input, if needed (shows screen keyboard on some platforms)
    if(ImGui::GetIO().WantTextInput && !isTextInputActive())
        startTextInput();
    else if(!ImGui::GetIO().WantTextInput && isTextInputActive())
        stopTextInput();

    buildUI();

    // requires updated ImGuiIntegration
    //imgui.updateApplicationCursor(*this);

    // state required for imgui rendering
    // alpha blending, scissor, no culling, no depth test

    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                   GL::Renderer::BlendFunction::OneMinusSourceAlpha);

    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    imgui.drawFrame();

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::Blending);
}

// this is not necessarily called for DPI changes (at least not on Windows).
// app might have to be restarted to reflect new settings.
void ImGuiApplication::viewportEvent(Magnum::Platform::Application::ViewportEvent& event)
{
    GL::defaultFramebuffer.setViewport({ { 0, 0 }, event.framebufferSize() });
    imgui.relayout(uiSize(), event.windowSize(), event.framebufferSize());
}

void ImGuiApplication::keyPressEvent(KeyEvent& event)
{
    if(imgui.handleKeyPressEvent(event))
        event.setAccepted();
}

void ImGuiApplication::keyReleaseEvent(KeyEvent& event)
{
    if(imgui.handleKeyReleaseEvent(event))
        event.setAccepted();
}

void ImGuiApplication::mousePressEvent(MouseEvent& event)
{
    if(imgui.handleMousePressEvent(event))
        event.setAccepted();
}

void ImGuiApplication::mouseReleaseEvent(MouseEvent& event)
{
    if(imgui.handleMouseReleaseEvent(event))
        event.setAccepted();
}

void ImGuiApplication::mouseMoveEvent(MouseMoveEvent& event)
{
    if(imgui.handleMouseMoveEvent(event))
        event.setAccepted();
}

void ImGuiApplication::mouseScrollEvent(MouseScrollEvent& event)
{
    if(imgui.handleMouseScrollEvent(event))
        event.setAccepted();
}

void ImGuiApplication::textInputEvent(TextInputEvent& event)
{
    if(imgui.handleTextInputEvent(event))
        event.setAccepted();
}

Magnum::Vector2 ImGuiApplication::uiSize() const
{
    return Magnum::Vector2(windowSize()) / dpiScaling();
}

void ImGuiApplication::setFont(const char* fontFile, float pixels)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig fontConfig;
    fontConfig.GlyphRanges = io.Fonts->GetGlyphRangesDefault();
    io.Fonts->AddFontFromFileTTF(fontFile, pixels * framebufferSize().x() / uiSize().x(), &fontConfig);
    // update font atlas
    imgui.relayout(uiSize(), windowSize(), framebufferSize());
}
