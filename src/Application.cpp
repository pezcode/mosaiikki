#include "Application.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Shaders/Phong.h>
#include <Corrade/Utility/Arguments.h>

namespace GL = Magnum::GL;

Application::Application(const Arguments& arguments) :
    Magnum::Platform::Application(arguments)
{
    Corrade::Utility::Arguments parser;
    parser.parse(arguments.argc, arguments.argv);
}

void Application::drawEvent()
{
    GL::defaultFramebuffer.clearColor(Magnum::Color4::cyan());

    swapBuffers();
}
