#include "ReconstructionShader.h"

#include <Magnum/GL/Shader.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/MeshTools/FullScreenTriangle.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <algorithm>
#include <cassert>

using namespace Magnum;

ReconstructionShader::ReconstructionShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    triangle(NoCreate),
    colorSamplerLocation(-1),
    depthSamplerLocation(-1),
    currentFrameUniformLocation(-1),
    resolutionChangedUniformLocation(-1),
    viewportUniformLocation(-1),
    prevViewProjectionUniformLocation(-1),
    invViewProjectionUniformLocation(-1),
    debugShowSamplesUniformLocation(-1),
    viewport({ 0, 0 }),
    resolutionChanged(true),
    prevViewProjection(Magnum::Math::IdentityInit)
{

}

ReconstructionShader::ReconstructionShader() : GL::AbstractShaderProgram()
{
    triangle = MeshTools::fullScreenTriangle(GLVersion);

    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("ReconstructionShader.vert"));
    frag.addSource(rs.get("ReconstructionShader.frag"));

    // possibly parallel compilation
    GL::Shader::compile({ vert, frag });
    attachShaders({ vert, frag });
    link();

    colorSamplerLocation = uniformLocation("color");
    depthSamplerLocation = uniformLocation("depth");
    currentFrameUniformLocation = uniformLocation("currentFrame");
    resolutionChangedUniformLocation = uniformLocation("resolutionChanged");
    viewportUniformLocation = uniformLocation("viewport");
    prevViewProjectionUniformLocation = uniformLocation("prevViewProjection");
    invViewProjectionUniformLocation = uniformLocation("invViewProjection");
    debugShowSamplesUniformLocation = uniformLocation("debugShowSamples");

    setResolutionChanged(true);
}

ReconstructionShader& ReconstructionShader::bindColor(GL::MultisampleTexture2DArray& attachment)
{
    setUniform(colorSamplerLocation, TextureUnits::Color);
    attachment.bind(TextureUnits::Color);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindDepth(GL::MultisampleTexture2DArray& attachment)
{
    setUniform(depthSamplerLocation, TextureUnits::Depth);
    attachment.bind(TextureUnits::Depth);
    return *this;
}

ReconstructionShader& ReconstructionShader::setCurrentFrame(Int currentFrame)
{
    setUniform(currentFrameUniformLocation, currentFrame);
    return *this;
}

ReconstructionShader& ReconstructionShader::setCameraInfo(SceneGraph::Camera3D& camera)
{
    resolutionChanged = viewport != camera.viewport();
    viewport = camera.viewport();

    const Matrix4 viewProjection = camera.projectionMatrix() * camera.cameraMatrix();

    setUniform(viewportUniformLocation, viewport);
    setUniform(prevViewProjectionUniformLocation, prevViewProjection);
    setUniform(invViewProjectionUniformLocation, viewProjection.inverted());

    prevViewProjection = viewProjection;

    return *this;
}

ReconstructionShader& ReconstructionShader::setDebugShowSamples(DebugSamples samples)
{
    setUniform(debugShowSamplesUniformLocation, samples);
    return *this;
}

ReconstructionShader& ReconstructionShader::setResolutionChanged(bool changed)
{
    setUniform(resolutionChangedUniformLocation, changed);
    return *this;
}

void ReconstructionShader::draw()
{
    setResolutionChanged(resolutionChanged);
    triangle.draw(*this);
}
