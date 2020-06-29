// gl_SampleID
// core in 4.0
#extension GL_ARB_sample_shading : require

// textureGather
// core in 4.0
//#extension GL_ARB_texture_gather : require

uniform sampler2D depth; // full-resolution velocity pass depth

// downsample depth buffer to quarter-size 2X multisampled depth
// reads each patch of 2x2 fragment depths, finds the max value (to match depth function of less/less-or-equal)
// and writes the same depth to all 2 samples

void main()
{
	ivec2 halfCoords = ivec2(floor(gl_FragCoord.xy));
    ivec2 coords = halfCoords << 1;

	/*
	float depth1 = texelFetch(depth, coords + ivec2(0, 0), 0).x;
	float depth2 = texelFetch(depth, coords + ivec2(0, 1), 0).x;
	float depth3 = texelFetch(depth, coords + ivec2(1, 0), 0).x;
	float depth4 = texelFetch(depth, coords + ivec2(1, 1), 0).x;

	float d = max(max(depth1, depth2), max(depth3, depth4));
	*/

	/*
	// this would perform linear interpolation, not useful here (and wrong for depth values)
	vec2 viewport = vec2(textureSize(depth, 0));
	vec2 texCoords = vec2(coords) / viewport;
	vec2 offset1 = vec2(0.25, 0.25) / viewport;
	vec2 offset2 = vec2(0.75, 0.75) / viewport;

	float depth1 = textureGather(depth, texCoords + offset1).x;
	float depth2 = textureGather(depth, texCoords + offset2).x;

	float d = max(depth1, depth2);
	*/

	float d = texelFetch(depth, coords + ivec2(1 - gl_SampleID, 1 - gl_SampleID), 0).x;

	// without this offset, some samples can fail the depth test even for less-or-equal
	// assuming this has to do with floating point imprecision
	const float bias = 0.00;

	gl_FragDepth = d + bias;
}
