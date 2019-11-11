#include "Application.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Arguments.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Containers/ArrayView.h>

namespace GL = Magnum::GL;
using namespace Corrade;

Application::Application(const Arguments& arguments) :
    ImGuiApplication(arguments)
{
    Utility::Arguments parser;
    parser.parse(arguments.argc, arguments.argv);

    const char* fontName = "Roboto-Regular.ttf";
    const Containers::ArrayView<const char> font = Utility::Resource("font").getRaw(fontName);
    setFont(font.data(), font.size(), 15.0f);
}

void Application::drawEvent()
{
    // Ubuntu purple (Mid aubergine)
    // https://design.ubuntu.com/brand/colour-palette/
    const Magnum::Color3 clearColor = Magnum::Color3(0x5E, 0x27, 0x50) / 255.0f;
    GL::Renderer::setClearColor(clearColor);
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

    ImGuiApplication::drawEvent();
}

void Application::buildUI()
{
    ImGui::ShowTestWindow();
}
