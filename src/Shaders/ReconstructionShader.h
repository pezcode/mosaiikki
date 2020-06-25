#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/SceneGraph/Camera.h>
#include "Options.h"

class ReconstructionShader : public Magnum::GL::AbstractShaderProgram
{
public:
    ReconstructionShader(Magnum::NoCreateT);
    ReconstructionShader();

    ReconstructionShader& bindColor(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindDepth(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindVelocity(Magnum::GL::Texture2D& attachment);
    ReconstructionShader& setCurrentFrame(Magnum::Int currentFrame);
    // camera should be const, but camera.cameraMatrix() is not :(
    ReconstructionShader& setCameraInfo(Magnum::SceneGraph::Camera3D& camera, float nearPlane, float farPlane);
    ReconstructionShader& setOptions(const Options::Reconstruction& options);

    // normally you call mesh.draw(shader)
    // but we supply our own mesh for a fullscreen pass
    void draw();

private:
    using Magnum::GL::AbstractShaderProgram::drawTransformFeedback;
    using Magnum::GL::AbstractShaderProgram::dispatchCompute;

    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Magnum::GL::Mesh triangle;

    enum TextureUnits : Magnum::Int
    {
        Color = 0,
        Depth,
        Velocity
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
