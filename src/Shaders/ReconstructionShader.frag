// uniform buffer
// core in 3.1
#extension GL_ARB_uniform_buffer_object : require
// sampler2DMS
// core in 3.2
#extension GL_ARB_texture_multisample : require
// layout(location = ...)
// core in 3.3
#extension GL_ARB_explicit_attrib_location : require

// Reference:
// https://software.intel.com/en-us/articles/checkerboard-rendering-for-real-time-upscaling-on-intel-integrated-graphics

// quarter-res 2X multisampled textures
// two layers: even / odd (jittered)
uniform sampler2DMSArray color;
uniform sampler2DMSArray depth;

// full-res screen-space velocity buffer
// .z is a mask for moving objects
uniform sampler2D velocity;

layout(std140) uniform OptionsBlock
{
	mat4 prevViewProjection;
	mat4 invViewProjection;
	ivec2 viewport;
	float near;
	float far;
	int currentFrame; // is the current frame even or odd? (-> index into color and depth array layers)
	bool cameraParametersChanged;
	int flags;
	float depthTolerance;
};

// bit masks for flags
// this should match the ones in ReconstructionShader::setOptions
#define USE_VELOCITY_BUFFER     (1 << 0)
#define ASSUME_OCCLUSION        (1 << 1)

#define DEBUG_SHOW_SAMPLES      (1 << 2)
#define DEBUG_SHOW_EVEN_SAMPLES (1 << 3)
#define DEBUG_SHOW_VELOCITY     (1 << 4)
#define DEBUG_SHOW_COLORS       (1 << 5)

#define OPTION_SET(OPT) ((flags & OPT) != 0)
#ifdef DEBUG
#define DEBUG_OPTION_SET(OPT) OPTION_SET(OPT)
#else
#define DEBUG_OPTION_SET(OPT) (false)
#endif

layout(location = COLOR_OUTPUT_ATTRIBUTE_LOCATION) out vec4 fragColor;

/*
each quarter-res pixel corresponds to 4 pixels (quadrants) in the full-res output
each quarter-res pixel has two MSAA samples at fixed positions

quadrants:
+---+---+
| 2 | 3 |
+---+---+
| 0 | 1 |
+---+---+

sample positions:
+---+---+
|   | 0 |
+---+---+
| 1 |   |
+---+---+

the odd frames' viewport is jittered half a pixel (= one full-res pixel) to the right
so the sample positions overlap for full coverage across two frames

even:
    +---+---+
    |   | 0 |
    +---+---+
    | 1 |   |
    +---+---+
 
odd:
    +---+---+
    |   | A |
    +---+---+
    | B |   |
    +---+---+
 
combined:
    +---+---+
    | A | 0 |
+---+---+---+
| B | 1 |   |
+---+---+---+
*/

int calculateQuadrant(ivec2 pixelCoords)
{
	return (pixelCoords.x & 1) + (pixelCoords.y & 1) * 2;
}

vec4 fetchQuadrant(sampler2DMSArray tex, ivec2 coords, int quadrant)
{
	switch(quadrant)
	{
		default:
		case 0: // (x, y, even/odd), sample
			return texelFetch(tex, ivec3(coords, 0), 1);
		case 1:
			return texelFetch(tex, ivec3(coords + ivec2(1, 0), 1), 1);
		case 2:
			return texelFetch(tex, ivec3(coords, 1), 0);
		case 3:
			return texelFetch(tex, ivec3(coords, 0), 0);
	}
}

/*
quadrants to evaluate when averaging values around a quadrant:

0:
   +---+---+
   | 0 |   |
---+---+---+
 1 | X | 1 |
---+---+---+
   | 0 |

1:
   +---+---+
   |   | 0 |
   +---+---+---
   | 1 | X | 1
   +---+---+---
       | 0 |

2:
   | 1 |
---+---+---+
 0 | X | 0 |
---+---+---+
   | 1 |   |
   +---+---+

3:
       | 1 |
   +---+---+---
   | 0 | X | 0
   +---+---+---
   |   | 1 |
   +---+---+
*/

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

const ivec2 directionOffsets[4*4] = ivec2[4*4](
	// quadrant 0
	ivec2( 0,  0), // up
	ivec2( 0, -1), // down
	ivec2(-1,  0), // left
	ivec2( 0,  0), // right
	// quadrant 1
	ivec2( 0,  0),
	ivec2( 0, -1),
	ivec2( 0,  0),
	ivec2(+1,  0),
	// quadrant 2
	ivec2( 0, +1),
	ivec2( 0,  0),
	ivec2(-1,  0),
	ivec2( 0,  0),
	// quadrant 3
	ivec2( 0, +1),
	ivec2( 0,  0),
	ivec2( 0,  0),
	ivec2(+1,  0)
);

const ivec4 directionQuadrants[4] = ivec4[4](
	// quadrant 0
	ivec4(ivec2(2), ivec2(1)), // up/down, left/right
	// quadrant 1
	ivec4(ivec2(3), ivec2(0)),
	// quadrant 2
	ivec4(ivec2(0), ivec2(3)),
	// quadrant 3
	ivec4(ivec2(1), ivec2(2))
);

// tonemapping operator for combining HDR colors to prevent bright samples from dominating the result
// https://gpuopen.com/learn/optimized-reversible-tonemapper-for-resolve/

vec4 tonemap(vec4 color)
{
	return color / (max(color.r, max(color.g, color.b)) + 1.0);
}

vec4 undoTonemap(vec4 color)
{
	return color / (1.0 - max(color.r, max(color.g, color.b)));
}

vec4 fetchColorAverage(ivec2 coords, int quadrant)
{
	int k = quadrant * 4;
	vec4 result =
		tonemap(fetchQuadrant(color, coords + directionOffsets[k + UP   ], directionQuadrants[quadrant][UP   ])) +
		tonemap(fetchQuadrant(color, coords + directionOffsets[k + DOWN ], directionQuadrants[quadrant][DOWN ])) +
		tonemap(fetchQuadrant(color, coords + directionOffsets[k + LEFT ], directionQuadrants[quadrant][LEFT ])) +
		tonemap(fetchQuadrant(color, coords + directionOffsets[k + RIGHT], directionQuadrants[quadrant][RIGHT]));
	return undoTonemap(result * 0.25);
}

// undo depth projection
// assumes a projection transformation produced by Matrix4::perspectiveProjection with finite far plane
// solve for view space z: (z(n+f) + 2nf)/(-z(n-f)) = 2w-1
// w = window space depth [0;1]
// 2w-1 = NDC space depth [-1;1]
float screenToViewDepth(float depth)
{
	return (far * near) / ((far * depth) - far - (near * depth));
}

// returns averaged depth in view space
// depth buffer values are non-linear, averaging those produces incorrect results
float fetchDepthAverage(ivec2 coords, int quadrant)
{
	int k = quadrant * 4;
	float result =
		screenToViewDepth(fetchQuadrant(depth, coords + directionOffsets[k + UP   ], directionQuadrants[quadrant][UP   ]).x) +
		screenToViewDepth(fetchQuadrant(depth, coords + directionOffsets[k + DOWN ], directionQuadrants[quadrant][DOWN ]).x) +
		screenToViewDepth(fetchQuadrant(depth, coords + directionOffsets[k + LEFT ], directionQuadrants[quadrant][LEFT ]).x) +
		screenToViewDepth(fetchQuadrant(depth, coords + directionOffsets[k + RIGHT], directionQuadrants[quadrant][RIGHT]).x);
	return result * 0.25;
}

ivec2 reprojectPixel(ivec2 coords, float depth)
{
	vec2 screen = vec2(coords) + 0.5; // gl_FragCoord x/y are located at half-pixel centers, undo the flooring
	vec3 ndc = vec3(screen / viewport, depth) * 2.0 - 1.0; // z: [0;1] -> [-1;1]
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
		ivec2(3, 0), // even
		ivec2(2, 1) // odd
	);

	// debug output: velocity buffer
	if(DEBUG_OPTION_SET(DEBUG_SHOW_VELOCITY))
	{
		vec2 vel = texelFetch(velocity, coords, 0).xy;
		fragColor = vec4(abs(vel * 255.0), 0.0, 1.0);
		return;
	}

	// debug output: checkered frame
	if(DEBUG_OPTION_SET(DEBUG_SHOW_SAMPLES))
	{
		int sampleFrame = OPTION_SET(DEBUG_SHOW_EVEN_SAMPLES) ? 0 : 1;
		ivec2 boardQuadrants = FRAME_QUADRANTS[sampleFrame];
		if(any(equal(vec2(quadrant), boardQuadrants)))
			fragColor = fetchQuadrant(color, halfCoords, quadrant);
		else
			fragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	ivec2 currentQuadrants = FRAME_QUADRANTS[currentFrame];

	// was this pixel rendered with the most recent frame?
	// -> just use it
	if(any(equal(ivec2(quadrant), currentQuadrants)))
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

	bool possiblyOccluded = false;

	// find pixel position in previous frame

	ivec2 oldCoords = coords;
	bool velocityFromDepth = true;

	// for fully general results, sample from a velocity buffer
	if(OPTION_SET(USE_VELOCITY_BUFFER))
	{
		vec3 velocity = texelFetch(velocity, coords, 0).xyz;
		// z is a mask for dynamic objects
		if(velocity.z > 0.0)
		{
			vec2 offset = velocity.xy * viewport;
			oldCoords = ivec2(floor(gl_FragCoord.xy - offset));
			velocityFromDepth = false;
		}
		else
		{
			// force occlusion check to prevent ghosting when the previously
			// occluded pixel is at the far plane
			// if we only check for quarter-pixel movement, we'd see (0,0) movement in
			// that case and directly use the old frame's, but we need to average

			// TODO this causes jittering around object edges for perfectly still scenes
			// because now the average is taken even if there is nothing being occluded

			// similarly for velocityFromDepth, where a pixel at the far plane is close enough
			// to fail the depth occlusion check because it's in the neighborhood of the edge
			possiblyOccluded = true;
		}
	}

	// if we're not using a velocity buffer or the object is static, reproject using the camera transformation
	if(velocityFromDepth)
	{
		float z = fetchQuadrant(depth, halfCoords, quadrant).x;
		if(z < 1.0)
			oldCoords = reprojectPixel(coords, z);
		else
			possiblyOccluded = true;
	}

	// is the previous position outside the screen?
	if(any(lessThan(oldCoords, ivec2(0, 0))) || any(greaterThanEqual(oldCoords, viewport)))
	{
		if(DEBUG_OPTION_SET(DEBUG_SHOW_COLORS))
			fragColor = vec4(1.0, 1.0, 0.0, 1.0);
		else
			fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	ivec2 oldHalfCoords = oldCoords >> 1;
	int oldQuadrant = calculateQuadrant(oldCoords);

	// is the previous position not in an old frame quadrant?
	// this happens when any movement cancelled the jitter
	// -> there's no shading information
	ivec2 oldQuadrants = FRAME_QUADRANTS[1 - currentFrame];
	if(!any(equal(ivec2(oldQuadrant), oldQuadrants)))
	{
		if(DEBUG_OPTION_SET(DEBUG_SHOW_COLORS))
			fragColor = vec4(0.0, 1.0, 1.0, 1.0);
		else
			fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	// check for occlusion if the old position was in a different quarter-res pixel
	if(any(greaterThan(abs(oldHalfCoords - halfCoords), ivec2(0))))
	{
		possiblyOccluded = true;
	}

	// check for occlusion
	bool occluded = false;
	if(possiblyOccluded)
	{
		// simple variant: always assume occlusion
		if(OPTION_SET(ASSUME_OCCLUSION))
		{
			occluded = true;
		}
		else // more correct heuristic: check depth against current depth average
		{
			float currentDepthAverage = fetchDepthAverage(halfCoords, quadrant);
			float oldDepth = fetchQuadrant(depth, oldHalfCoords, oldQuadrant).x;
			// fetchDepthAverage returns average of linear view space depth
			oldDepth = screenToViewDepth(oldDepth);
			occluded = abs(currentDepthAverage - oldDepth) >= depthTolerance;
		}
	}

	if(occluded)
	{
		if(DEBUG_OPTION_SET(DEBUG_SHOW_COLORS))
			fragColor = vec4(1.0, 0.0, 1.0, 1.0);
		else
			fragColor = fetchColorAverage(halfCoords, quadrant);
		return;
	}

	// no occlusion, directly use the old color
	fragColor = fetchQuadrant(color, oldHalfCoords, oldQuadrant);
}
