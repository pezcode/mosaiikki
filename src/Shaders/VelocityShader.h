#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Shaders/GenericGL.h>
#include <Magnum/Math/Matrix4.h>
#include <Corrade/Containers/EnumSet.h>

class VelocityShader : public Magnum::GL::AbstractShaderProgram
{
public:
    typedef Magnum::Shaders::GenericGL3D::TransformationMatrix TransformationMatrix;
    typedef Magnum::GL::Attribute<TransformationMatrix::Location + TransformationMatrix::Vectors, Magnum::Matrix4>
        OldTransformationMatrix;

    enum : Magnum::UnsignedInt
    {
        VelocityOutput = Magnum::Shaders::GenericGL3D::ColorOutput
    };

    enum class Flag : Magnum::UnsignedShort
    {
        /**
         * Instanced transformation. Retrieves a per-instance
         * transformation and old transformation matrix from the
         * TransformationMatrix / OldTransformationMatrix attributes and
         * uses them together with matrices coming from
         * setTransformationMatrix() and setOldTransformationMatrix() (first
         * the per-instance, then the uniform matrix).
         */
        InstancedTransformation = 1 << 0
    };

    typedef Corrade::Containers::EnumSet<Flag> Flags;

    explicit VelocityShader(Magnum::NoCreateT);
    explicit VelocityShader(const Flags flags = {});

    Flags flags() const
    {
        return _flags;
    };

    VelocityShader& setTransformationMatrix(const Magnum::Matrix4& transformationMatrix);
    VelocityShader& setOldTransformationMatrix(const Magnum::Matrix4& oldTransformationMatrix);

    VelocityShader& setProjectionMatrix(const Magnum::Matrix4& projectionMatrix);
    VelocityShader& setOldProjectionMatrix(const Magnum::Matrix4& oldProjectionMatrix);

private:
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Flags _flags;

    Magnum::Int transformationMatrixUniform = -1;
    Magnum::Int oldTransformationMatrixUniform = -1;
    Magnum::Int projectionMatrixUniform = -1;
    Magnum::Int oldProjectionMatrixUniform = -1;
};

CORRADE_ENUMSET_OPERATORS(VelocityShader::Flags)
