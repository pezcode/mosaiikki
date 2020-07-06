#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/Shaders/Generic.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Corrade/Containers/EnumSet.h>
#include "Options.h"

class ReconstructionShader : public Magnum::GL::AbstractShaderProgram
{
public:
    enum : Magnum::UnsignedInt
    {
        ColorOutput = Magnum::Shaders::Generic3D::ColorOutput
    };

    enum class Flag : Magnum::UnsignedShort
    {
        // Debug output (configured through setOptions)
        Debug = 1 << 0
    };

    typedef Corrade::Containers::EnumSet<Flag> Flags;

    explicit ReconstructionShader(Magnum::NoCreateT);
    explicit ReconstructionShader(const Flags flags);

    Flags flags() const
    {
        return _flags;
    };

    ReconstructionShader& bindColor(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindDepth(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindVelocity(Magnum::GL::Texture2D& attachment);
    ReconstructionShader& setCurrentFrame(Magnum::Int currentFrame);
    ReconstructionShader& setCameraInfo(Magnum::SceneGraph::Camera3D& camera, float nearPlane, float farPlane);
    ReconstructionShader& setOptions(const Options::Reconstruction& options);

    // normally you call mesh.draw(shader)
    // but we supply our own mesh for a fullscreen pass
    void draw();

private:
    using Magnum::GL::AbstractShaderProgram::draw;
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Flags _flags;

    Magnum::GL::Mesh triangle;

    enum : Magnum::Int
    {
        ColorTextureUnit = 0,
        DepthTextureUnit = 1,
        VelocityTextureUnit = 2
    };

    Magnum::Int optionsBlock;
    Magnum::GL::Buffer optionsBuffer;

    struct OptionsBufferData
    {
        Magnum::Matrix4 prevViewProjection = Magnum::Matrix4(Magnum::Math::IdentityInit);
        Magnum::Matrix4 invViewProjection = Magnum::Matrix4(Magnum::Math::IdentityInit);
        Magnum::Vector2i viewport = { 0, 0 };
        float near = 0.01f;
        float far = 50.0f;
        GLint currentFrame = 0;
        GLuint cameraParametersChanged = false;
        GLint flags = 0;
        GLfloat depthTolerance = 0.01f;
    };
    OptionsBufferData optionsData;

    Magnum::Vector2i viewport;
    Magnum::Matrix4 projection;
    Magnum::Matrix4 prevViewProjection;
};

CORRADE_ENUMSET_OPERATORS(ReconstructionShader::Flags)
