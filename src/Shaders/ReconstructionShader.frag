// sampler2DMS
// core in 3.2
#extension GL_ARB_texture_multisample : require
// layout(location = ...)
// core in 3.3
#extension GL_ARB_explicit_attrib_location : require

uniform sampler2DMSArray color; // even / odd (jittered)
uniform sampler2DMSArray depth;

uniform sampler2D velocity;

uniform int currentFrame;

uniform bool resolutionChanged;
uniform ivec2 viewport;
uniform mat4 prevViewProjection;
uniform mat4 invViewProjection;

uniform int debugShowSamples = 0;
uniform bool debugShowVelocity = false;

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

vec4 undoTonemap(vec4 color)
{
	return color;
}

vec4 tonemap(vec4 color)
{
	return color;
}

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

void calculateAverageOffsets(out ivec2 offsets[4], out ivec4 quadrants)
{

}

vec4 fetchColorAverage(ivec2 coords, int quadrant)
{
	const ivec2 offsets[4] = ivec2[4](
		ivec2(0, 0),
		ivec2(0, 0),
		ivec2(0, 0),
		ivec2(0, 0)
	);

	const ivec4 quadrants = ivec4(
		0,
		0,
		0,
		0
	);

	vec4 result =
		undoTonemap(fetchQuadrant(color, coords + offsets[UP   ], quadrants[UP   ])) +
		undoTonemap(fetchQuadrant(color, coords + offsets[DOWN ], quadrants[DOWN ])) +
		undoTonemap(fetchQuadrant(color, coords + offsets[LEFT ], quadrants[LEFT ])) +
		undoTonemap(fetchQuadrant(color, coords + offsets[RIGHT], quadrants[RIGHT]));
	return tonemap(result * 0.25);
}

vec4 fetchDepthAverage(ivec2 coords, int quadrant)
{
	const ivec2 offsets[4] = ivec2[4](
		ivec2(0, 0),
		ivec2(0, 0),
		ivec2(0, 0),
		ivec2(0, 0)
	);

	const ivec4 quadrants = ivec4(
		0,
		0,
		0,
		0
	);

	vec4 result =
		fetchQuadrant(depth, coords + offsets[UP   ], quadrants[UP   ]) +
		fetchQuadrant(depth, coords + offsets[DOWN ], quadrants[DOWN ]) +
		fetchQuadrant(depth, coords + offsets[LEFT ], quadrants[LEFT ]) +
		fetchQuadrant(depth, coords + offsets[RIGHT], quadrants[RIGHT]);
	return result * 0.25;
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
	if(debugShowVelocity)
	{
		vec2 vel = texture2D(velocity, gl_FragCoord.xy / viewport).xy * viewport;
		fragColor = vec4(abs(vel), 0.0, 1.0);
		return;
	}

	ivec2 fullCoords = ivec2(floor(gl_FragCoord.xy));
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
		fetchColorAverage(halfCoords, quadrant);
		return;
	}

	// find pixel position in previous frame
	// for fully general results, we'd sample from a velocity buffer here
	// for now, reproject pixel to get delta from camera transformation

	//float z = fetchQuadrant(depth, halfCoords, quadrant).x;
	//ivec2 oldCoords = reprojectPixel(fullCoords, z);

	vec2 diff = texture2D(velocity, gl_FragCoord.xy / viewport).xy * viewport;
	ivec2 oldCoords = ivec2(floor(gl_FragCoord.xy - diff));

	ivec2 oldHalfCoords = oldCoords >> 1;
	int oldQuadrant = calculateQuadrant(oldCoords);

	// is the previous pixel in an old frame quadrant?
	ivec2 oldQuadrants = CURRENT_SAMPLES[1 - currentFrame];
	if(any(equal(vec2(oldQuadrant), oldQuadrants)))
	{
		fragColor = fetchQuadrant(color, oldHalfCoords, oldQuadrant);
		return;
	}

	// previous pixel position is not in old frame quadrants, use current average
	//fragColor = fetchColorAverage(halfCoords, quadrant);

	fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
