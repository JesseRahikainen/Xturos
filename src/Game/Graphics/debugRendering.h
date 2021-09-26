#ifndef DEBUG_RENDERING_H
#define DEBUG_RENDERING_H

#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Graphics/color.h"

#define MAX_DEBUG_VERTS 2048 * 4
#if MAX_DEBUG_VERTS % 2 == 1
	#error "debugRendering MAX_VERTS needs to be divisible by 2."
#endif

typedef struct {
	Vector3 pos;
	Color color;
	unsigned int camFlags;
} DebugVertex;

int debugRenderer_Init( void );

// Empties the list of debug things to draw.
void debugRenderer_ClearVertices( void );

// Some basic debug drawing functions. The alpha value of colors are ignored.
//  Returns 0 on success. Prints an error message to the log if it fails and returns -1.
int debugRenderer_AABB( unsigned int camFlags, Vector2 topLeft, Vector2 size, Color color );
int debugRenderer_Line( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Color color );
int debugRenderer_Circle( unsigned int camFlags, Vector2 center, float radius, Color color );
int debugRenderer_Ellipse( unsigned int camFlags, Vector2 center, float horizRadius, float vertRadius, Color color );
int debugRenderer_Triangle( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Vector2 pThree, Color color );

// Draw all the debug lines.
void debugRenderer_Render( void );

#endif // inclusion guard