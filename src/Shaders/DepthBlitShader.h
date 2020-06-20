#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>

class DepthBlitShader : public Magnum::GL::AbstractShaderProgram
{
public:
    DepthBlitShader(Magnum::NoCreateT);
    DepthBlitShader();

    DepthBlitShader& bindDepth(Magnum::GL::Texture2D& attachment);

    // normally you call mesh.draw(shader)
    // but we supply our own mesh for a fullscreen pass
    void draw();

private:
    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Magnum::GL::Mesh triangle;

    enum TextureUnits : Magnum::Int
    {
        Depth = 0
    };

    Magnum::Int depthSampler;
};
