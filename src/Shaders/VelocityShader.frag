in vec4 clipPos;
in vec4 oldClipPos;
out vec2 velocity;

void main()
{
	vec2 distance = (clipPos.xy / clipPos.w) - (oldClipPos.xy / oldClipPos.w);
	velocity = distance * 0.5; // [-1;1]
}
