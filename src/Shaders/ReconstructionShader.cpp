#include "ReconstructionShader.h"

#include <Magnum/GL/Shader.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/MeshTools/FullScreenTriangle.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>

using namespace Magnum;

ReconstructionShader::ReconstructionShader(NoCreateT) :
    GL::AbstractShaderProgram(NoCreate),
    triangle(NoCreate),
    colorSampler(-1),
    depthSampler(-1),
    velocitySampler(-1),
    currentFrameUniform(-1),
    resolutionChangedUniform(-1),
    viewportUniform(-1),
    prevViewProjectionUniform(-1),
    invViewProjectionUniform(-1),
    debugShowSamplesUniform(-1),
    debugShowVelocityUniform(-1),
    viewport({ 0, 0 }),
    resolutionChanged(true),
    prevViewProjection(Math::IdentityInit)
{
}

ReconstructionShader::ReconstructionShader() :
    GL::AbstractShaderProgram(), viewport({ 0, 0 }), resolutionChanged(true), prevViewProjection(Math::IdentityInit)
{
    triangle = MeshTools::fullScreenTriangle(GLVersion);

    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    // TODO
    // use Resource::overrideGroup to load resources from files if they exist
    // together with FileWatcher this will allow shader hot-reloading
    // https://doc.magnum.graphics/corrade/classCorrade_1_1Utility_1_1Resource.html#a06013f7ed2126fc3f81bcfde849faf69
    // https://doc.magnum.graphics/corrade/classCorrade_1_1Utility_1_1FileWatcher.html

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("ReconstructionShader.vert"));
    frag.addSource(rs.get("ReconstructionShader.frag"));

    // possibly parallel compilation
    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile ReconstructionShader");
    attachShaders({ vert, frag });
    link();

    colorSampler = uniformLocation("color");
    depthSampler = uniformLocation("depth");
    velocitySampler = uniformLocation("velocity");
    currentFrameUniform = uniformLocation("currentFrame");
    resolutionChangedUniform = uniformLocation("resolutionChanged");
    viewportUniform = uniformLocation("viewport");
    prevViewProjectionUniform = uniformLocation("prevViewProjection");
    invViewProjectionUniform = uniformLocation("invViewProjection");
    debugShowSamplesUniform = uniformLocation("debugShowSamples");
    debugShowVelocityUniform = uniformLocation("debugShowVelocity");
}

ReconstructionShader& ReconstructionShader::bindColor(GL::MultisampleTexture2DArray& attachment)
{
    setUniform(colorSampler, TextureUnits::Color);
    attachment.bind(TextureUnits::Color);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindDepth(GL::MultisampleTexture2DArray& attachment)
{
    setUniform(depthSampler, TextureUnits::Depth);
    attachment.bind(TextureUnits::Depth);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindVelocity(GL::Texture2D& attachment)
{
    setUniform(velocitySampler, TextureUnits::Velocity);
    attachment.bind(TextureUnits::Velocity);
    return *this;
}

ReconstructionShader& ReconstructionShader::setCurrentFrame(Int currentFrame)
{
    setUniform(currentFrameUniform, currentFrame);
    return *this;
}

ReconstructionShader& ReconstructionShader::setCameraInfo(SceneGraph::Camera3D& camera)
{
    resolutionChanged = viewport != camera.viewport();
    viewport = camera.viewport();
    const Matrix4 viewProjection = camera.projectionMatrix() * camera.cameraMatrix();

    setUniform(viewportUniform, viewport);
    setUniform(prevViewProjectionUniform, prevViewProjection);
    setUniform(invViewProjectionUniform, viewProjection.inverted());

    prevViewProjection = viewProjection;

    return *this;
}

ReconstructionShader& ReconstructionShader::setDebugShowSamples(DebugSamples samples)
{
    setUniform(debugShowSamplesUniform, samples);
    return *this;
}

ReconstructionShader& ReconstructionShader::setDebugShowVelocity(bool show)
{
    setUniform(debugShowVelocityUniform, show);
    return *this;
}

ReconstructionShader& ReconstructionShader::setResolutionChanged(bool changed)
{
    setUniform(resolutionChangedUniform, changed);
    return *this;
}

void ReconstructionShader::draw()
{
    setResolutionChanged(resolutionChanged);
    AbstractShaderProgram::draw(triangle);
}
