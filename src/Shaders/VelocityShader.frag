in vec4 clipPos;
in vec4 oldClipPos;
out vec4 velocity;

void main()
{
	vec2 distance = (clipPos.xy / clipPos.w) - (oldClipPos.xy / oldClipPos.w);
	distance *= 0.5; // [-1;1]
	//distance = vec2(0.0, 0.0);
	velocity = vec4(distance, 1.0, 1.0);
}
