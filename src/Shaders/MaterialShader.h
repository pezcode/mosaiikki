#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Math/Matrix4.h>

class MaterialShader : public Magnum::GL::AbstractShaderProgram
{
public:
    MaterialShader(Magnum::NoCreateT);
    MaterialShader();

    MaterialShader& setOldTransformation(const Magnum::Matrix4& oldTransformation);
    MaterialShader& setTransformation(const Magnum::Matrix4& transformation);
    MaterialShader& setOldProjection(const Magnum::Matrix4& oldProjection);
    MaterialShader& setProjection(const Magnum::Matrix4& projection);

private:
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Magnum::Int oldModelViewProjectionUniform;
    Magnum::Int modelViewProjectionUniform;

    Magnum::Matrix4 oldProjection;
    Magnum::Matrix4 projection;
};
