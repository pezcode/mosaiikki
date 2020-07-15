#include "VelocityShader.h"

#include <Magnum/GL/Shader.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/FormatStl.h>

using namespace Magnum;

VelocityShader::VelocityShader(NoCreateT) : GL::AbstractShaderProgram(NoCreate) { }

VelocityShader::VelocityShader(const Flags flags) : _flags(flags)
{
    GL::Shader vert(GLVersion, GL::Shader::Type::Vertex);
    GL::Shader frag(GLVersion, GL::Shader::Type::Fragment);

    Utility::Resource rs("shaders");

    vert.addSource(flags & Flag::InstancedTransformation ? "#define INSTANCED_TRANSFORMATION\n" : "");
    vert.addSource(Utility::formatString("#define POSITION_ATTRIBUTE_LOCATION {}\n"
                                         "#define TRANSFORMATION_ATTRIBUTE_LOCATION {}\n"
                                         "#define OLD_TRANSFORMATION_ATTRIBUTE_LOCATION {}\n",
                                         Shaders::Generic3D::Position::Location,
                                         TransformationMatrix::Location,
                                         OldTransformationMatrix::Location));
    vert.addSource(rs.get("VelocityShader.vert"));

    frag.addSource(Utility::formatString("#define VELOCITY_OUTPUT_ATTRIBUTE_LOCATION {}\n", VelocityOutput));
    frag.addSource(rs.get("VelocityShader.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({ vert, frag }));
    attachShaders({ vert, frag });
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    transformationMatrixUniform = uniformLocation("transformationMatrix");
    oldTransformationMatrixUniform = uniformLocation("oldTransformationMatrix");
    projectionMatrixUniform = uniformLocation("projectionMatrix");
    oldProjectionMatrixUniform = uniformLocation("oldProjectionMatrix");
}

VelocityShader& VelocityShader::setTransformationMatrix(const Magnum::Matrix4& transformationMatrix)
{
    setUniform(transformationMatrixUniform, transformationMatrix);
    return *this;
}

VelocityShader& VelocityShader::setOldTransformationMatrix(const Magnum::Matrix4& oldTransformationMatrix)
{
    setUniform(oldTransformationMatrixUniform, oldTransformationMatrix);
    return *this;
}

VelocityShader& VelocityShader::setProjectionMatrix(const Magnum::Matrix4& projectionMatrix)
{
    setUniform(projectionMatrixUniform, projectionMatrix);
    return *this;
}

VelocityShader& VelocityShader::setOldProjectionMatrix(const Magnum::Matrix4& oldProjectionMatrix)
{
    setUniform(oldProjectionMatrixUniform, oldProjectionMatrix);
    return *this;
}
