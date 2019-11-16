#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Mesh.h>

class ReconstructionShader : public Magnum::GL::AbstractShaderProgram
{
public:
    ReconstructionShader(Magnum::NoCreateT);
    ReconstructionShader();

    ReconstructionShader& bindCurrentColorAttachment(Magnum::GL::MultisampleTexture2D& attachment);
    ReconstructionShader& bindPreviousColorAttachment(Magnum::GL::MultisampleTexture2D& attachment);

    // normally you call mesh.draw(shader)
    // but we supply our own mesh for a fullscreen pass
    void draw();

private:
    Magnum::GL::Mesh triangle;

    static constexpr Magnum::Int CurrentColorTextureUnit = 0;
    static constexpr Magnum::Int PreviousColorTextureUnit = 1;

    Magnum::Int currentColorSamplerLocation;
    Magnum::Int previousColorSamplerLocation;
};
