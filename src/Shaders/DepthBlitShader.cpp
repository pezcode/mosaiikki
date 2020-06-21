#include "DepthBlitShader.h"

#include <Magnum/GL/Shader.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/MeshTools/FullScreenTriangle.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

DepthBlitShader::DepthBlitShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    triangle(NoCreate),
    depthSampler(-1)
{
}

DepthBlitShader::DepthBlitShader() :
    GL::AbstractShaderProgram()
{
    triangle = MeshTools::fullScreenTriangle(GLVersion);

    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("DepthBlitShader.vert"));
    frag.addSource(rs.get("DepthBlitShader.frag"));

    // possibly parallel compilation
    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile DepthBlitShader", );
    attachShaders({ vert, frag });
    link();

    depthSampler = uniformLocation("depth");
}

DepthBlitShader& DepthBlitShader::bindDepth(GL::Texture2D& attachment)
{
    setUniform(depthSampler, TextureUnits::Depth);
    attachment.bind(TextureUnits::Depth);
    return *this;
}

void DepthBlitShader::draw()
{
    AbstractShaderProgram::draw(triangle);
}
