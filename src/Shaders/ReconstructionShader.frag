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

uniform bool cameraParametersChanged;
uniform ivec2 viewport;
uniform mat4 prevViewProjection;
uniform mat4 invViewProjection;
uniform vec4 depthTransform;

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

// the odd frames are jittered half a pixel to the right
// so the sample positions overlap into the even frames
// to get e.g. quadrant 1, 

int calculateQuadrant(ivec2 pixelCoords)
{
	return (pixelCoords.x & 1) + (pixelCoords.y & 1) * 2;
}

vec4 fetchQuadrant(sampler2DMSArray tex, ivec2 coords, int quadrant)
{
	switch(quadrant)
	{
		default:
		case 0:
			return texelFetch(tex, ivec3(coords, 0), 1);
		case 1:
			return texelFetch(tex, ivec3(coords + ivec2(1, 0), 1), 1);
		case 2:
			return texelFetch(tex, ivec3(coords, 1), 0);
		case 3:
			return texelFetch(tex, ivec3(coords, 0), 0);
	}
}

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

void calculateAverageOffsets(out ivec2 offsets[4], out ivec4 quadrants)
{

}

vec4 tonemap(vec4 color)
{
	return color;
}

// undo tonemap operation
// averaging color values is only correct in linear space
vec4 undoTonemap(vec4 color)
{
	return color;
}

// undo depth projection
// averaging depth values is only correct in linear view space
float screenToViewDepth(float depth)
{
	return (depth * depthTransform.x + depthTransform.y) / (depth * depthTransform.z + depthTransform.w);
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

// returns averaged depth in view space
float fetchDepthAverage(ivec2 coords, int quadrant)
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

	float result =
		fetchQuadrant(depth, coords + offsets[UP   ], quadrants[UP   ]).x +
		fetchQuadrant(depth, coords + offsets[DOWN ], quadrants[DOWN ]).x +
		fetchQuadrant(depth, coords + offsets[LEFT ], quadrants[LEFT ]).x +
		fetchQuadrant(depth, coords + offsets[RIGHT], quadrants[RIGHT]).x;
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
	ivec2 coords = ivec2(floor(gl_FragCoord.xy));
    ivec2 halfCoords = coords >> 1;
	int quadrant = calculateQuadrant(coords);

	const ivec2 FRAME_QUADRANTS[2] = ivec2[](
		ivec2(3, 0),
		ivec2(2, 1)
	);

	ivec2 currentQuadrants = FRAME_QUADRANTS[currentFrame];

	// debug output: velocity buffer
	if(debugShowVelocity)
	{
		vec2 vel = texelFetch(velocity, coords, 0).xy;
		fragColor = vec4(abs(vel * 255.0), 0.0, 1.0);
		return;
	}

	// debug output: checkered frame
	if(debugShowSamples != 0)
	{
		ivec2 boardQuadrants = FRAME_QUADRANTS[debugShowSamples - 1];
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
	if(cameraParametersChanged)
	{
		fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	// find pixel position in previous frame
	// for fully general results, we sample from a velocity buffer here
	// if we only have a moving camera, we can reproject using the camera transformation

	//float z = fetchQuadrant(depth, halfCoords, quadrant).x;
	//ivec2 oldCoords = reprojectPixel(coords, z);

	vec2 diff = texelFetch(velocity, coords, 0).xy * viewport;
	ivec2 oldCoords = ivec2(floor(gl_FragCoord.xy - diff));

	if(any(lessThan(oldCoords, ivec2(0, 0))) || any(greaterThanEqual(oldCoords, viewport)))
	{
		// previous position is outside the screen
		// -> interpolate
		fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	ivec2 oldHalfCoords = oldCoords >> 1;
	int oldQuadrant = calculateQuadrant(oldCoords);

	// is the previous pixel in an old frame quadrant?
	ivec2 oldQuadrants = FRAME_QUADRANTS[1 - currentFrame];
	if(!any(equal(vec2(oldQuadrant), oldQuadrants)))
	{
		// it's not, movement cancelled jitter so there's no shading information
		// -> interpolate
		fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	// check if old frame's pixel is occluded

	// simple variant: always assume occlusion if anything moved more than a half-res pixel
	bool occluded = any(greaterThan(abs(oldCoords - coords), ivec2(1)));
	// more correct heuristic: check depth against current depth average
	if(false)
	{
		const float DEPTH_TOLERANCE = 0.01;
		float currentDepthAverage = fetchDepthAverage(halfCoords, quadrant);
		float oldDepth = fetchQuadrant(depth, oldHalfCoords, oldQuadrant).x;
		occluded = abs(currentDepthAverage - oldDepth) >= DEPTH_TOLERANCE;
	}

	if(occluded)
	{
		fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	fragColor = fetchQuadrant(color, oldHalfCoords, oldQuadrant);
}
