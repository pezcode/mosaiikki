#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Mesh.h>
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
    ReconstructionShader& setCameraInfo(Magnum::SceneGraph::Camera3D& camera);
    ReconstructionShader& setOptions(const Options::Reconstruction& options);

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
    Magnum::Int optionsUniform;

    Magnum::Vector2i viewport;
    Magnum::Matrix4 projection;
    Magnum::Matrix4 prevViewProjection;
};
