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
	gl_FragDepth = texelFetch(depth, coords + ivec2(1 - gl_SampleID), 0).x;
}
