#include "DepthBlitShader.h"

#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

DepthBlitShader::DepthBlitShader(NoCreateT) : GL::AbstractShaderProgram(NoCreate) { }

DepthBlitShader::DepthBlitShader()
{
    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("DepthBlitShader.vert"));
    frag.addSource(rs.get("DepthBlitShader.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({ vert, frag }));
    attachShaders({ vert, frag });
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(uniformLocation("depth"), DepthTextureUnit);
}

DepthBlitShader& DepthBlitShader::bindDepth(GL::Texture2D& attachment)
{
    attachment.bind(DepthTextureUnit);
    return *this;
}
