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
    cameraParametersChangedUniform(-1),
    viewportUniform(-1),
    prevViewProjectionUniform(-1),
    invViewProjectionUniform(-1),
    optionsUniform(-1),
    viewport({ 0, 0 }),
    projection(Math::IdentityInit),
    prevViewProjection(Math::IdentityInit)
{
}

ReconstructionShader::ReconstructionShader() :
    GL::AbstractShaderProgram(),
    viewport({ 0, 0 }),
    projection(Math::IdentityInit),
    prevViewProjection(Math::IdentityInit)
{
    triangle = MeshTools::fullScreenTriangle(GLVersion);

    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");
    vert.addSource(rs.get("ReconstructionShader.vert"));
    frag.addSource(rs.get("ReconstructionShader.frag"));

    // possibly parallel compilation
    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile ReconstructionShader", );
    attachShaders({ vert, frag });
    link();

    colorSampler = uniformLocation("color");
    depthSampler = uniformLocation("depth");
    velocitySampler = uniformLocation("velocity");
    currentFrameUniform = uniformLocation("currentFrame");
    cameraParametersChangedUniform = uniformLocation("cameraParametersChanged");
    viewportUniform = uniformLocation("viewport");
    prevViewProjectionUniform = uniformLocation("prevViewProjection");
    invViewProjectionUniform = uniformLocation("invViewProjection");
    optionsUniform = uniformLocation("options");
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
    // TODO this doesn't work when using (jittered) projection matrix to calculate velocity in the shader
    // current code removes the camera jitter before calling this function
    bool projectionChanged = (projection - camera.projectionMatrix()).toVector() != Math::Vector<4 * 4, Float>(0.0f);
    bool cameraParametersChanged = viewport != camera.viewport() || projectionChanged;
    setUniform(cameraParametersChangedUniform, cameraParametersChanged);
    projection = camera.projectionMatrix();

    viewport = camera.viewport();
    setUniform(viewportUniform, viewport);

    const Matrix4 viewProjection = camera.projectionMatrix() * camera.cameraMatrix();
    setUniform(prevViewProjectionUniform, prevViewProjection);
    setUniform(invViewProjectionUniform, viewProjection.inverted());
    prevViewProjection = viewProjection;

    return *this;
}

ReconstructionShader& ReconstructionShader::setOptions(const Options::Reconstruction& options)
{
    GLint options_bitset = 0;
    if(options.assumeOcclusion)
        options_bitset |= (1 << 0);
    if(options.debug.showSamples != Options::Reconstruction::Debug::Samples::All)
        options_bitset |= (1 << 1);
    if(options.debug.showSamples == Options::Reconstruction::Debug::Samples::Even)
        options_bitset |= (1 << 2);
    if(options.debug.showVelocity)
        options_bitset |= (1 << 3);

    setUniform(optionsUniform, options_bitset);
    return *this;
}

void ReconstructionShader::draw()
{
    AbstractShaderProgram::draw(triangle);
}
