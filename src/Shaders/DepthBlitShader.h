#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>

class DepthBlitShader : public Magnum::GL::AbstractShaderProgram
{
public:
    explicit DepthBlitShader(Magnum::NoCreateT);
    explicit DepthBlitShader();

    DepthBlitShader& bindDepth(Magnum::GL::Texture2D& attachment);

private:
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    enum : Magnum::Int
    {
        DepthTextureUnit = 0
    };
};
