#include "ReconstructionShader.h"

#include <Magnum/GL/Version.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/MeshTools/FullScreenTriangle.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <algorithm>
#include <cassert>

using namespace Magnum;

ReconstructionShader::ReconstructionShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    triangle(NoCreate),
    currentColorSamplerLocation(0),
    previousColorSamplerLocation(0)
{

}

ReconstructionShader::ReconstructionShader() : GL::AbstractShaderProgram()
{
    triangle = MeshTools::fullScreenTriangle(GL::Version::GL330);

    currentColorSamplerLocation = uniformLocation("currentColor");
    previousColorSamplerLocation = uniformLocation("previousColor");

    GL::Shader vert(GL::Version::GL330, GL::Shader::Type::Vertex);
    GL::Shader frag(GL::Version::GL330, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("ReconstructionShader.vert"));
    frag.addSource(rs.get("ReconstructionShader.frag"));

    // parallel compilation
    assert(GL::Shader::compile({ vert, frag }));
    attachShaders({ vert, frag });
    assert(link());
}

ReconstructionShader& ReconstructionShader::bindCurrentColorAttachment(Magnum::GL::MultisampleTexture2D& attachment)
{
    setUniform(currentColorSamplerLocation, CurrentColorTextureUnit);
    attachment.bind(CurrentColorTextureUnit);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindPreviousColorAttachment(Magnum::GL::MultisampleTexture2D& attachment)
{
    setUniform(previousColorSamplerLocation, PreviousColorTextureUnit);
    attachment.bind(PreviousColorTextureUnit);
    return *this;
}

void ReconstructionShader::draw()
{
    triangle.draw(*this);
}
