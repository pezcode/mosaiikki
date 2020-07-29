// layout(location = ...)
// core in 3.3
#extension GL_ARB_explicit_attrib_location : require

uniform mat4 transformationMatrix = mat4(1.0);
uniform mat4 oldTransformationMatrix = mat4(1.0);

uniform mat4 projectionMatrix = mat4(1.0);
uniform mat4 oldProjectionMatrix = mat4(1.0);

layout(location = POSITION_ATTRIBUTE_LOCATION) in vec4 position;

#ifdef INSTANCED_TRANSFORMATION
layout(location = TRANSFORMATION_ATTRIBUTE_LOCATION) in mat4 instancedTransformationMatrix;
layout(location = OLD_TRANSFORMATION_ATTRIBUTE_LOCATION) in mat4 instancedOldTransformationMatrix;
#endif

out vec4 clipPos;
out vec4 oldClipPos;

void main()
{
    clipPos = projectionMatrix * transformationMatrix *
        #ifdef INSTANCED_TRANSFORMATION
        instancedTransformationMatrix *
        #endif
        position;

    oldClipPos = oldProjectionMatrix * oldTransformationMatrix *
        #ifdef INSTANCED_TRANSFORMATION
        instancedOldTransformationMatrix *
        #endif
        position;

    gl_Position = clipPos;
}
