#include "MaterialShader.h"

#include <Magnum/GL/Shader.h>
#include <Magnum/Shaders/Generic.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

MaterialShader::MaterialShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    oldModelViewProjectionUniform(-1),
    modelViewProjectionUniform(-1),
    oldProjection(Math::IdentityInit),
    projection(Math::IdentityInit)
{
}

MaterialShader::MaterialShader() : GL::AbstractShaderProgram()
{
    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("MaterialShader.vert"));
    frag.addSource(rs.get("MaterialShader.frag"));

    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile MaterialShader", );
    attachShaders({ vert, frag });
    link();

    oldModelViewProjectionUniform = uniformLocation("oldModelViewProjection");
    modelViewProjectionUniform = uniformLocation("modelViewProjection");
}

MaterialShader& MaterialShader::setOldTransformation(const Magnum::Matrix4& oldTransformationMatrix)
{
    setUniform(oldModelViewProjectionUniform, oldProjection * oldTransformationMatrix);
    return *this;
}

MaterialShader& MaterialShader::setTransformation(const Magnum::Matrix4& transformationMatrix)
{
    setUniform(modelViewProjectionUniform, projection * transformationMatrix);
    return *this;
}

MaterialShader& MaterialShader::setOldProjection(const Magnum::Matrix4& oldProjectionMatrix)
{
    oldProjection = oldProjectionMatrix;
    return *this;
}

MaterialShader& MaterialShader::setProjection(const Magnum::Matrix4& projectionMatrix)
{
    projection = projectionMatrix;
    return *this;
}
