#include "debugRendering.h"

#include <math.h>

#include "Graphics/Platform/debugRenderingPlatform.h"

#include "Math/matrix4.h"
#include "color.h"
#include "camera.h"

#include "System/platformLog.h"

#define NUM_CIRC_VERTS 64//8

static DebugVertex debugBuffer[MAX_DEBUG_VERTS];
static int lastDebugVert;

int debugRenderer_Init( void )
{
	if( !debugRendererPlatform_Init( sizeof( debugBuffer ) ) ) {
		return -1;
	}

	lastDebugVert = -1;

	return 0;
}

/*
Empties the list of debug things to draw.
*/
void debugRenderer_ClearVertices( void )
{
	lastDebugVert = -1;
}

/*
Some basic debug drawing functions.
  Returns 0 on success. Prints an error message to the log if it fails and returns -1.
*/
static int queueDebugVert( unsigned int camFlags, Vector2 pos, Color color )
{
	if( lastDebugVert >= ( MAX_DEBUG_VERTS - 1 ) ) {
		llog( LOG_VERBOSE, "Debug instruction queue full." );
		return -1;
	}

	++lastDebugVert;
	vec2ToVec3( &pos, 0.0f, &( debugBuffer[lastDebugVert].pos ) );
	debugBuffer[lastDebugVert].color = color;
	debugBuffer[lastDebugVert].color.a = 1.0f;
	debugBuffer[lastDebugVert].camFlags = camFlags;

	return 0;
}

int debugRenderer_AABB( unsigned int camFlags, Vector2 topLeft, Vector2 size, Color color )
{
	int fail = 0;
	Vector2 corners[4];
	corners[0] = topLeft;

	corners[1] = topLeft;
	corners[1].v[0] += size.v[0];

	vec2_Add( &topLeft, &size, &corners[2] );

	corners[3] = topLeft;
	corners[3].v[1] += size.v[1];

	for( int i = 0; i < 4; ++i ) {
		fail = fail || debugRenderer_Line( camFlags, corners[i], corners[(i+1)%4], color );
	}

	return -fail;
}

int debugRenderer_Line( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Color color )
{
	if( queueDebugVert( camFlags, pOne, color ) ) {
		return -1;
	}
	if( queueDebugVert( camFlags, pTwo, color ) ) {
		return -1;
	}

	return 0;
}

int debugRenderer_Circle( unsigned int camFlags, Vector2 center, float radius, Color color )
{
	int fail;
	Vector2 circPos[NUM_CIRC_VERTS];

	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		circPos[i].v[0] = center.v[0] + ( sinf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
		circPos[i].v[1] = center.v[1] + ( cosf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
	}

	fail = 0;
	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		fail = fail || debugRenderer_Line( camFlags, circPos[i], circPos[( i + 1 ) % NUM_CIRC_VERTS], color );
	}

	return -fail;
}

int debugRenderer_Ellipse( unsigned int camFlags, Vector2 center, float horizRadius, float vertRadius, Color color )
{
	int fail;
	Vector2 ellipsePos[NUM_CIRC_VERTS];

	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		ellipsePos[i].v[0] = center.v[0] + ( sinf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * horizRadius );
		ellipsePos[i].v[1] = center.v[1] + ( cosf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * vertRadius );
	}

	fail = 0;
	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		fail = fail || debugRenderer_Line( camFlags, ellipsePos[i], ellipsePos[( i + 1 ) % NUM_CIRC_VERTS], color );
	}

	return -fail;
}

int debugRenderer_Triangle( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Vector2 pThree, Color color )
{
	int fail = 0;
	fail = fail || debugRenderer_Line( camFlags, pOne, pTwo, color );
	fail = fail || debugRenderer_Line( camFlags, pOne, pThree, color );
	fail = fail || debugRenderer_Line( camFlags, pThree, pTwo, color );

	return -fail;
}

// Draw all the debug lines.
void debugRenderer_Render( void )
{
	debugRendererPlatform_Render( debugBuffer, lastDebugVert );
}