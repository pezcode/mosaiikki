#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Math/Matrix4.h>

class VelocityShader : public Magnum::GL::AbstractShaderProgram
{
public:
    VelocityShader(Magnum::NoCreateT);
    VelocityShader();

    VelocityShader& setOldTransformation(const Magnum::Matrix4& oldTransformation);
    VelocityShader& setTransformation(const Magnum::Matrix4& transformation);
    VelocityShader& setOldProjection(const Magnum::Matrix4& oldProjection);
    VelocityShader& setProjection(const Magnum::Matrix4& projection);

private:
    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;
    Magnum::Int oldModelViewProjectionUniform;
    Magnum::Int modelViewProjectionUniform;

    Magnum::Matrix4 oldProjection;
    Magnum::Matrix4 projection;
};
