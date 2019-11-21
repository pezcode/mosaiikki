// sampler2DMS
// core in 3.2
#extension GL_ARB_texture_multisample : require
// layout(location = ...)
// core in 3.3
#extension GL_ARB_explicit_attrib_location : require

uniform sampler2DMSArray color; // even / odd (jittered)
uniform sampler2DMSArray depth;

uniform int currentFrame;

uniform bool resolutionChanged;
uniform ivec2 viewport;
uniform mat4 viewProjection;
uniform mat4 prevInvViewProjection;

uniform int debugShowSamples;

layout(location = 0) out vec4 fragColor;

// https://software.intel.com/en-us/articles/checkerboard-rendering-for-real-time-upscaling-on-intel-integrated-graphics
// https://github.com/GameTechDev/DynamicCheckerboardRendering/blob/master/MiniEngine/ModelViewer/Shaders/CheckerboardColorResolveCS.hlsl

// D3D (original):

// quadrants:
// +---+---+
// | 0 | 1 |
// +---+---+
// | 2 | 3 |
// +---+---+

// samples:
// +---+---+
// | 1 |   |
// +---+---+
// |   | 0 |
// +---+---+

// GL:

// quadrants:
// +---+---+
// | 2 | 3 |
// +---+---+
// | 0 | 1 |
// +---+---+

// samples:
// +---+---+
// |   | 0 |
// +---+---+
// | 1 |   |
// +---+---+

int calculateQuadrant(ivec2 pixelCoords)
{
	return (pixelCoords.x & 1) + (pixelCoords.y & 1) * 2;
}

vec4 fetchQuadrant(sampler2DMSArray tex, ivec2 coords, int quadrant)
{
	/*
	if(quadrant == 0)
		return texelFetch(color[0], coords, 1);
	else if(quadrant == 1)
		return texelFetch(color[1], coords + ivec2(1, 0), 1);
	else if(quadrant == 2)
		return texelFetch(color[1], coords, 0);
	else if(quadrant == 3)
		return texelFetch(color[0], coords, 0);
	*/

	if(quadrant == 0)
		return texelFetch(tex, ivec3(coords, 0), 1);
	else if(quadrant == 1)
		return texelFetch(tex, ivec3(coords + ivec2(1, 0), 1), 1);
	else if(quadrant == 2)
		return texelFetch(tex, ivec3(coords, 1), 0);
	else if(quadrant == 3)
		return texelFetch(tex, ivec3(coords, 0), 0);
}

vec4 screen2World(vec4 coord)
{
	coord.xy /= viewport;
	vec4 ndc = coord * 2.0 - 1.0;
    vec4 eye = prevInvViewProjection * ndc;
    return eye / eye.w;
}

ivec2 reprojectPixel(ivec2 oldCoord, float depth)
{
	vec4 fragCoord = vec4(vec2(oldCoord), depth, 1.0);
	vec4 world = screen2World(fragCoord);
	vec4 ndc = viewProjection * world;
	ndc /= ndc.w;
	vec2 newCoord = ndc.xy * 0.5 + 0.5;
	return ivec2(floor(newCoord * viewport));
}

void main()
{
	ivec2 fullCoords = ivec2(gl_FragCoord.xy);
    ivec2 halfCoords = fullCoords >> 1;
	int quadrant = calculateQuadrant(fullCoords);

	const ivec2 CURRENT_SAMPLES[2] = ivec2[](
		ivec2(0, 3),
		ivec2(1, 2)
	);
	ivec2 currentQuadrants = CURRENT_SAMPLES[currentFrame];

	// debug output: checkered frame
	if(debugShowSamples != 0)
	{
		ivec2 boardQuadrants = CURRENT_SAMPLES[debugShowSamples - 1];
		if(any(equal(vec2(quadrant), boardQuadrants)))
			fragColor = fetchQuadrant(color, halfCoords, quadrant);
		else
			fragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	
	// was this pixel rendered with the most recent frame?
	// -> just use it
	if(any(equal(vec2(quadrant), currentQuadrants)))
	{
		fragColor = fetchQuadrant(color, halfCoords, quadrant);
		return;
	}

	// we have no old data, use average
	if(resolutionChanged)
	{
		// TODO
		fragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	
	float z = fetchQuadrant(depth, halfCoords, quadrant).x;
	ivec2 newHalfCoords = reprojectPixel(halfCoords, z);

	if(!all(equal(halfCoords, newHalfCoords)))
	{
		//fragColor = vec4(1.0, 0.0, 0.0, 1.0);
		//return;
	}

	//ivec2 newHalfCoords = halfCoords;

    fragColor = fetchQuadrant(color, newHalfCoords, quadrant);
}
