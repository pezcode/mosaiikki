#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>

class DepthBlitShader : public Magnum::GL::AbstractShaderProgram
{
public:
    explicit DepthBlitShader(Magnum::NoCreateT);
    explicit DepthBlitShader();

    DepthBlitShader& bindDepth(Magnum::GL::Texture2D& attachment);

    // normally you call _mesh.draw(shader)
    // but we supply our own _mesh for a fullscreen pass
    void draw();

private:
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Magnum::GL::Mesh triangle;

    enum TextureUnits : Magnum::Int
    {
        Depth = 0
    };
};
