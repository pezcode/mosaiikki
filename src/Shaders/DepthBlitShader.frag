// gl_SampleID
// core in 4.0
#extension GL_ARB_sample_shading : require

uniform sampler2D depth; // full-resolution velocity pass depth

// downsample depth buffer to quarter-size 2X multisampled depth

void main()
{
	ivec2 halfCoords = ivec2(floor(gl_FragCoord.xy));
    ivec2 coords = halfCoords << 1;

	// let each sample copy the depth value from the full screen pass
	// this works because we know exactly which pixels the samples fall on
	// requires per-sample shading, which is forced on by using gl_SampleID
	float d = texelFetch(depth, coords + ivec2(1 - gl_SampleID), 0).x;

	// without this offset, some samples can fail the depth test even for less-or-equal
	// most likely caused by floating point imprecision
	const float bias = 0.000001;

	gl_FragDepth = d + bias;
}
