// textureGather
// core in 4.0
#extension GL_ARB_texture_gather : require

uniform sampler2D depth; // full-resolution velocity pass depth

void main()
{
	ivec2 halfCoords = ivec2(floor(gl_FragCoord.xy));
    ivec2 coords = halfCoords << 1;

	float depth1 = texelFetch(depth, coords + ivec2(0, 0), 0).x;
	float depth2 = texelFetch(depth, coords + ivec2(0, 1), 0).x;
	float depth3 = texelFetch(depth, coords + ivec2(1, 0), 0).x;
	float depth4 = texelFetch(depth, coords + ivec2(1, 1), 0).x;

	float d = max(max(depth1, depth2), max(depth3, depth4));

//	float depth1 = texelFetch(depth, coords + ivec2(-1,  0), 0).x;
//	float depth2 = texelFetch(depth, coords + ivec2( 1,  0), 0).x;
//	float depth3 = texelFetch(depth, coords + ivec2( 0, -1), 0).x;
//	float depth4 = texelFetch(depth, coords + ivec2( 0,  1), 0).x;
//	float depth5 = texelFetch(depth, coords + ivec2( 0,  0), 0).x;
//	float depth6 = texelFetch(depth, coords + ivec2(-1,  1), 0).x;
//	float depth7 = texelFetch(depth, coords + ivec2( 1,  1), 0).x;
//	float depth8 = texelFetch(depth, coords + ivec2( 1, -1), 0).x;
//	float depth9 = texelFetch(depth, coords + ivec2(-1, -1), 0).x;
//
//	float d = max(max(max(depth1, depth2), max(depth3, depth4)), max(max(depth5, depth6), max(depth7, depth8)));
//	d = max(d, depth9);

//	vec2 viewport = vec2(textureSize(depth, 0));
//	vec2 texCoords = vec2(coords) / viewport;
//	vec2 offset1 = vec2(0.25, 0.25) / viewport;
//	vec2 offset2 = vec2(0.75, 0.75) / viewport;
//
//	float depth1 = textureGather(depth, texCoords + offset1).x;
//	float depth2 = textureGather(depth, texCoords + offset2).x;
//
//	float d = max(depth1, depth2);

	const float bias = 0.00001;

	gl_FragDepth = d + bias;
}
