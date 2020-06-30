// gl_SampleID
// core in 4.0
#extension GL_ARB_sample_shading : require

uniform sampler2D depth; // full-resolution velocity pass depth

// downsample depth buffer to quarter-size 2X multisampled depth

void main()
{
	ivec2 halfCoords = ivec2(floor(gl_FragCoord.xy));
    ivec2 coords = halfCoords << 1;

	// - read each patch of 2x2 fragment depths
	// - find the max value (to match depth function of less/less-or-equal)
	// - writes the same depth to all 2 samples
	float depth1 = texelFetch(depth, coords + ivec2(0, 0), 0).x;
	float depth2 = texelFetch(depth, coords + ivec2(0, 1), 0).x;
	float depth3 = texelFetch(depth, coords + ivec2(1, 0), 0).x;
	float depth4 = texelFetch(depth, coords + ivec2(1, 1), 0).x;

	float d = max(max(depth1, depth2), max(depth3, depth4));

	// alternative since we know the sample positions:
	// (requires per-sample shading, which can be forced on by using gl_SampleID)
	//float d = texelFetch(depth, coords + ivec2(1 - gl_SampleID), 0).x;

	// without this offset, some samples can fail the depth test even for less-or-equal
	// assuming this has to do with floating point imprecision
	const float bias = 0.00001;

	gl_FragDepth = d + bias;
}
