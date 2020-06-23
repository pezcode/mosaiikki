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
    optionsBlock(-1),
    optionsBuffer(NoCreate),
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
#ifdef CORRADE_IS_DEBUG_BUILD
    frag.addSource("#define DEBUG");
#endif
    frag.addSource(rs.get("ReconstructionShader.frag"));

    // possibly parallel compilation
    bool compiled = GL::Shader::compile({ vert, frag });
    CORRADE_ASSERT(compiled, "Failed to compile ReconstructionShader", );
    attachShaders({ vert, frag });
    link();

    colorSampler = uniformLocation("color");
    depthSampler = uniformLocation("depth");
    velocitySampler = uniformLocation("velocity");
    optionsBlock = uniformBlockIndex("OptionsBlock");

    optionsBuffer = GL::Buffer(GL::Buffer::TargetHint::Uniform, { optionsData }, GL::BufferUsage::DynamicDraw);
    optionsBuffer.setLabel("Checkerboard resolve uniform buffer");
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
    optionsData.currentFrame = currentFrame;
    return *this;
}

ReconstructionShader& ReconstructionShader::setCameraInfo(SceneGraph::Camera3D& camera, float nearPlane, float farPlane)
{
    bool projectionChanged = (projection - camera.projectionMatrix()).toVector() != Math::Vector<4 * 4, Float>(0.0f);
    optionsData.cameraParametersChanged = viewport != camera.viewport() || projectionChanged;
    projection = camera.projectionMatrix();

    viewport = camera.viewport();
    optionsData.viewport = viewport;
    optionsData.near = nearPlane;
    optionsData.far = farPlane;

    const Matrix4 viewProjection = projection * camera.cameraMatrix();
    optionsData.prevViewProjection = prevViewProjection;
    optionsData.invViewProjection = viewProjection.inverted();
    prevViewProjection = viewProjection;

    return *this;
}

ReconstructionShader& ReconstructionShader::setOptions(const Options::Reconstruction& options)
{
    GLint flags_bitset = 0;
    if(options.createVelocityBuffer)
        flags_bitset |= (1 << 0);
    if(options.assumeOcclusion)
        flags_bitset |= (1 << 1);
    if(options.debug.showSamples != Options::Reconstruction::Debug::Samples::Combined)
        flags_bitset |= (1 << 2);
    if(options.debug.showSamples == Options::Reconstruction::Debug::Samples::Even)
        flags_bitset |= (1 << 3);
    if(options.debug.showVelocity)
        flags_bitset |= (1 << 4);
    if(options.debug.showColors)
        flags_bitset |= (1 << 5);

    optionsData.flags = flags_bitset;
    optionsData.depthTolerance = options.depthTolerance;
    return *this;
}

void ReconstructionShader::draw()
{
    // orphan the old buffer to prevent stalls
    optionsBuffer.setData({ optionsData }, GL::BufferUsage::DynamicDraw);
    //optionsBuffer.setSubData(0, { optionsData });
    optionsBuffer.bind(GL::Buffer::Target::Uniform, optionsBlock);
    AbstractShaderProgram::draw(triangle);
}
