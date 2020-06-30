in vec4 clipPos;
in vec4 oldClipPos;
out vec4 velocity;

void main()
{
	vec2 distance = (clipPos.xy / clipPos.w) - (oldClipPos.xy / oldClipPos.w);
	// scale to [-1;1] so we can simply multiply by the viewport size to get screenspace velocity
	// requires 16-bit float framebuffer attachment
	distance *= 0.5;
	velocity = vec4(distance, 1.0, 1.0);
}
