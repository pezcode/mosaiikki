#include "VelocityShader.h"

#include <Magnum/GL/Shader.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

VelocityShader::VelocityShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    oldModelViewProjectionUniform(-1),
    modelViewProjectionUniform(-1),
    oldProjection(Math::IdentityInit),
    projection(Math::IdentityInit)
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

VelocityShader& VelocityShader::setOldTransformation(const Magnum::Matrix4& oldTransformationMatrix)
{
    setUniform(oldModelViewProjectionUniform, oldProjection * oldTransformationMatrix);
    return *this;
}

VelocityShader& VelocityShader::setTransformation(const Magnum::Matrix4& transformationMatrix)
{
    setUniform(modelViewProjectionUniform, projection * transformationMatrix);
    return *this;
}

VelocityShader& VelocityShader::setOldProjection(const Magnum::Matrix4& oldProjectionMatrix)
{
    oldProjection = oldProjectionMatrix;
    return *this;
}

VelocityShader& VelocityShader::setProjection(const Magnum::Matrix4& projectionMatrix)
{
    projection = projectionMatrix;
    return *this;
}
