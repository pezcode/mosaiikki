uniform mat4 oldModelViewProjection;
uniform mat4 modelViewProjection;

in vec4 position;
out vec4 clipPos;
out vec4 oldClipPos;

void main()
{
	clipPos = modelViewProjection * position;
	oldClipPos = oldModelViewProjection * position;
	gl_Position = clipPos;
}
