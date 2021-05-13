#include "ReconstructionShader.h"

#include "ReconstructionOptions.h"
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/FormatStl.h>

using namespace Magnum;

ReconstructionShader::ReconstructionShader(NoCreateT) : GL::AbstractShaderProgram(NoCreate), optionsBuffer(NoCreate) { }

ReconstructionShader::ReconstructionShader(const Flags flags) : _flags(flags)
{
    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");

    vert.addSource(rs.getString("ReconstructionShader.vert"));

    frag.addSource(flags & Flag::Debug ? "#define DEBUG\n" : "");
    frag.addSource(Utility::formatString("#define COLOR_OUTPUT_ATTRIBUTE_LOCATION {}\n", ColorOutput));
    frag.addSource(rs.getString("ReconstructionOptions.h"));
    frag.addSource(rs.getString("ReconstructionShader.frag"));

    // possibly parallel compilation
    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({ vert, frag }));
    attachShaders({ vert, frag });
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(uniformLocation("color"), ColorTextureUnit);
    setUniform(uniformLocation("depth"), DepthTextureUnit);
    setUniform(uniformLocation("velocity"), VelocityTextureUnit);

    optionsBlock = uniformBlockIndex("OptionsBlock");

    optionsBuffer = GL::Buffer(GL::Buffer::TargetHint::Uniform, { optionsData }, GL::BufferUsage::DynamicDraw);
    optionsBuffer.setLabel("Checkerboard resolve uniform buffer");
}

ReconstructionShader& ReconstructionShader::bindColor(GL::MultisampleTexture2DArray& attachment)
{
    attachment.bind(ColorTextureUnit);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindDepth(GL::MultisampleTexture2DArray& attachment)
{
    attachment.bind(DepthTextureUnit);
    return *this;
}

ReconstructionShader& ReconstructionShader::bindVelocity(GL::Texture2D& attachment)
{
    attachment.bind(VelocityTextureUnit);
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
        flags_bitset |= OPTION_USE_VELOCITY_BUFFER;
    if(options.assumeOcclusion)
        flags_bitset |= OPTION_ASSUME_OCCLUSION;
    if(options.differentialBlending)
        flags_bitset |= OPTION_DIFFERENTIAL_BLENDING;
    if(options.debug.showSamples != Options::Reconstruction::Debug::Samples::Combined)
        flags_bitset |= OPTION_DEBUG_SHOW_SAMPLES;
    if(options.debug.showSamples == Options::Reconstruction::Debug::Samples::Even)
        flags_bitset |= OPTION_DEBUG_SHOW_EVEN_SAMPLES;
    if(options.debug.showVelocity)
        flags_bitset |= OPTION_DEBUG_SHOW_VELOCITY;
    if(options.debug.showColors)
        flags_bitset |= OPTION_DEBUG_SHOW_COLORS;

    optionsData.flags = flags_bitset;
    optionsData.depthTolerance = options.depthTolerance;
    return *this;
}

ReconstructionShader& ReconstructionShader::setBuffer()
{
    // orphan the old buffer to prevent stalls
    optionsBuffer.setData({ optionsData }, GL::BufferUsage::DynamicDraw);
    //optionsBuffer.setSubData(0, { optionsData });
    optionsBuffer.bind(GL::Buffer::Target::Uniform, optionsBlock);
    return *this;
}
