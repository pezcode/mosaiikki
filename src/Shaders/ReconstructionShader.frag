uniform sampler2DMS currentColor;
uniform sampler2DMS previousColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 coords = ivec2(gl_FragCoord.xy * 0.5);
	// gl_SampleID?
    vec3 sample0 = texelFetch(currentColor, coords, 0).xyz;
    fragColor = vec4(sample0, 1.0);
}
