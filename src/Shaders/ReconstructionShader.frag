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
uniform mat4 prevViewProjection;
uniform mat4 invViewProjection;

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

// sample positions:
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

ivec2 reprojectPixel(ivec2 coords, float depth)
{
	vec2 screen = vec2(coords) + 0.5; // gl_FragCoord x/y are located at half-pixel centers, undo the flooring
	vec3 ndc = vec3(screen / viewport, depth) * 2.0 - 1.0;
	vec4 clip = vec4(ndc, 1.0);
	vec4 world = invViewProjection * clip;
	world /= world.w;
	clip = prevViewProjection * world;
	ndc = clip.xyz / clip.w;
	screen = (ndc.xy * 0.5 + 0.5) * viewport;
	coords = ivec2(floor(screen));
	return coords;
}

void main()
{
	ivec2 fullCoords = ivec2(gl_FragCoord.xy);
    ivec2 halfCoords = fullCoords >> 1;
	int quadrant = calculateQuadrant(fullCoords);

	const ivec2 CURRENT_SAMPLES[2] = ivec2[](
		ivec2(3, 0),
		ivec2(2, 1)
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

	// find pixel position in previous frame
	// for fully general results, we'd sample from a velocity buffer here
	// for now, reproject pixel to get delta from camera transformation
	float z = fetchQuadrant(depth, halfCoords, quadrant).x;
	ivec2 oldCoords = reprojectPixel(fullCoords, z);
	ivec2 oldHalfCoords = oldCoords >> 1;
	int oldQuadrant = calculateQuadrant(oldCoords);

	if(!all(equal(fullCoords, oldCoords)))
	{
		fragColor = vec4(1.0, 0.0, 0.0, 1.0);
		return;
	}

    fragColor = fetchQuadrant(color, oldHalfCoords, oldQuadrant);
}
