#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Math/Matrix4.h>

class VelocityShader : public Magnum::GL::AbstractShaderProgram
{
public:
    VelocityShader(Magnum::NoCreateT);
    VelocityShader();

    VelocityShader& setOldModelViewProjection(const Magnum::Matrix4& mvp);
    VelocityShader& setModelViewProjection(const Magnum::Matrix4& mvp);

private:
    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;
    Magnum::Int oldModelViewProjectionUniform;
    Magnum::Int modelViewProjectionUniform;
};
