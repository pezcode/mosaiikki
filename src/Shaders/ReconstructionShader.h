#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/SceneGraph/Camera.h>

class ReconstructionShader : public Magnum::GL::AbstractShaderProgram
{
public:
    ReconstructionShader(Magnum::NoCreateT);
    ReconstructionShader();

    enum DebugSamples : Magnum::Int
    {
        Disabled = 0,
        Even,
        Odd
    };

    ReconstructionShader& bindColor(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindDepth(Magnum::GL::MultisampleTexture2DArray& attachment);
    ReconstructionShader& bindVelocity(Magnum::GL::Texture2D& attachment);
    ReconstructionShader& setCurrentFrame(Magnum::Int currentFrame);
    // camera should be const, but camera.cameraMatrix() is not :(
    ReconstructionShader& setCameraInfo(Magnum::SceneGraph::Camera3D& camera);
    ReconstructionShader& setDebugShowSamples(DebugSamples samples);
    ReconstructionShader& setDebugShowVelocity(bool show);

    // normally you call mesh.draw(shader)
    // but we supply our own mesh for a fullscreen pass
    void draw();

private:
    static constexpr Magnum::GL::Version GLVersion = Magnum::GL::Version::GL300;

    Magnum::GL::Mesh triangle;

    enum TextureUnits : Magnum::Int
    {
        Color = 0,
        Depth,
        Velocity
    };

    Magnum::Int colorSampler;
    Magnum::Int depthSampler;
    Magnum::Int velocitySampler;
    Magnum::Int currentFrameUniform;
    Magnum::Int cameraParametersChangedUniform;
    Magnum::Int viewportUniform;
    Magnum::Int prevViewProjectionUniform;
    Magnum::Int invViewProjectionUniform;
    Magnum::Int debugShowSamplesUniform;
    Magnum::Int debugShowVelocityUniform;

    Magnum::Vector2i viewport;
    Magnum::Matrix4 projection;
    Magnum::Matrix4 prevViewProjection;
};
