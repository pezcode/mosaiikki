#include "VelocityShader.h"

#include <Magnum/GL/Shader.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

VelocityShader::VelocityShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate), oldModelViewProjectionUniform(-1), modelViewProjectionUniform(-1)
{
}

VelocityShader::VelocityShader() : GL::AbstractShaderProgram()
{
    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("VelocityShader.vert"));
    frag.addSource(rs.get("VelocityShader.frag"));

    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile VelocityShader");
    attachShaders({ vert, frag });
    link();

    oldModelViewProjectionUniform = uniformLocation("oldModelViewProjection");
    modelViewProjectionUniform = uniformLocation("modelViewProjection");
}

VelocityShader& VelocityShader::setOldModelViewProjection(const Magnum::Matrix4& mvp)
{
    setUniform(oldModelViewProjectionUniform, mvp);
    return *this;
}

VelocityShader& VelocityShader::setModelViewProjection(const Magnum::Matrix4& mvp)
{
    setUniform(modelViewProjectionUniform, mvp);
    return *this;
}
