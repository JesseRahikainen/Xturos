#include "triRendering.h"

#include <stdlib.h>

#include "Graphics/triRendering_DataTypes.h"
#include "Graphics/Platform/triRenderingPlatform.h"
#include "Graphics/Platform/graphicsPlatform.h"
#include "Math/matrix4.h"
#include "camera.h"
#include "System/platformLog.h"
#include "Math/mathUtil.h"
#include "System/memory.h"


/*
Ok, so what do we want to optimize for?
I'd think transferring memory.
So we have the vertices we transfer at the beginning of the rendering
Once that is done we generate index buffers to represent what each camera can see
*/
#define MAX_SOLID_TRIS 2048
#define MAX_TRANSPARENT_TRIS 2048
#define MAX_STENCIL_TRIS 32

#define MAX_TRIS ( MAX( MAX_SOLID_TRIS, MAX( MAX_TRANSPARENT_TRIS, MAX_STENCIL_TRIS ) ) )

TriangleList solidTriangles;
TriangleList transparentTriangles;
TriangleList stencilTriangles;

#define Z_ORDER_OFFSET ( 1.0f / (float)( 2 * ( MAX_TRIS + 1 ) ) )

TriVert triVert( Vector2 pos, Vector2 uv, Color col )
{
	TriVert v;
	v.pos = pos;
	v.uv = uv;
	v.col = col;
	return v;
}

int triRenderer_LoadShaders( void )
{
	if( !triPlatform_LoadShaders( ) ) {
		return -1;
	}

	return 0;
}

static int initTriList( TriangleList* triList, TriType listType )
{
	if( !triPlatform_InitTriList( triList, listType ) ) {
		return -1;
	}

	triList->lastIndexBufferIndex = -1;
	triList->lastTriIndex = -1;

	return 0;
}

/*
Initializes all the stuff needed for rendering the triangles.
 Returns a value < 0 if there's a problem.
*/
int triRenderer_Init( )
{
	if( triRenderer_LoadShaders( ) < 0 ) {
		return -1;
	}

	solidTriangles.triCount = MAX_SOLID_TRIS;
	solidTriangles.vertCount = MAX_SOLID_TRIS * 3;

	transparentTriangles.triCount = MAX_TRANSPARENT_TRIS;
	transparentTriangles.vertCount = MAX_TRANSPARENT_TRIS * 3;

	stencilTriangles.triCount = MAX_STENCIL_TRIS;
	stencilTriangles.vertCount = MAX_STENCIL_TRIS * 3;

	llog( LOG_INFO, "Creating triangle lists." );
	if( ( initTriList( &solidTriangles, TT_SOLID ) < 0 ) ||
		( initTriList( &transparentTriangles, TT_TRANSPARENT ) < 0 ) ||
		( initTriList( &stencilTriangles, TT_STENCIL ) < 0 ) ) {
		return -1;
	}

	return 0;
}

static bool isTriAxisSeparating( Vector2* p0, Vector2* p1, Vector2* p2 )
{
	float temp;
	Vector2 projTriPt0, projTriPt2;
	float triPt0Dot, triPt2Dot;
	float triMin, triMax;
	float quadMin, quadMax;

	Vector2 orthoAxis;
	vec2_Subtract( p0, p1, &orthoAxis );
	temp = orthoAxis.x;
	orthoAxis.x = -orthoAxis.y;
	orthoAxis.y = temp;
	vec2_Normalize( &orthoAxis ); // is this necessary?

	// since we're generating it from p0 and p1 that means they should both project to the same
	//  value on the axis, so we only need to project p0 and p2
	vec2_ProjOnto( p0, &orthoAxis, &projTriPt0 );
	vec2_ProjOnto( p2, &orthoAxis, &projTriPt2 );
	triPt0Dot = vec2_DotProduct( &orthoAxis, &projTriPt0 );
	triPt2Dot = vec2_DotProduct( &orthoAxis, &projTriPt2 );

	if( triPt0Dot > triPt2Dot ) {
		triMin = triPt2Dot;
		triMax = triPt0Dot;
	} else {
		triMin = triPt0Dot;
		triMax = triPt2Dot;
	}

	// now find min and max for AABB on this axis, we know all points will be <+/-1,+/-1>
	float dotPP = orthoAxis.x + orthoAxis.y;
	float dotPN = orthoAxis.x - orthoAxis.y;
	float dotNN = -orthoAxis.x - orthoAxis.y;
	float dotNP = -orthoAxis.x + orthoAxis.y;

	quadMin = MIN( dotPP, MIN( dotPN, MIN( dotNN, dotNP ) ) );
	quadMax = MAX( dotPP, MAX( dotPN, MAX( dotNN, dotNP ) ) );

	return ( FLT_LT( quadMax, triMin ) || FLT_GT( quadMin, triMax ) );
}

// test to see if the triangle intersects the AABB centered on (0,0) with sides of length 2
static bool testTriangle( Vector2* p0, Vector2* p1, Vector2* p2 )
{
	// we'll need an optimized version of the SAT test, since we always know the position and size
	//  of the AABB we can precompute things

	// first test to see if there is a horizontal or vertical axis that separates
	if( FLT_LT( p0->x, -1.0f ) && FLT_LT( p1->x, -1.0f ) && FLT_LT( p2->x, -1.0f ) ) return false;
	if( FLT_GT( p0->x, 1.0f ) && FLT_GT( p1->x, 1.0f ) && FLT_GT( p2->x, 1.0f ) ) return false;

	if( FLT_LT( p0->y, -1.0f ) && FLT_LT( p1->y, -1.0f ) && FLT_LT( p2->y, -1.0f ) ) return false;
	if( FLT_GT( p0->y, 1.0f ) && FLT_GT( p1->y, 1.0f ) && FLT_GT( p2->y, 1.0f ) ) return false;

	// now test to see if the segments of the triangle generate a separating axis
	if( isTriAxisSeparating( p0, p1, p2 ) ) return false;
	if( isTriAxisSeparating( p1, p2, p0 ) ) return false;
	if( isTriAxisSeparating( p2, p0, p1 ) ) return false;

	return true;
}

static int addTriangle( TriangleList* triList, TriVert vert0, TriVert vert1, TriVert vert2,
	ShaderType shader, PlatformTexture texture, PlatformTexture extraTexture, float floatVal0, int clippingID, uint32_t camFlags, int8_t depth )
{
	// test to see if the triangle can be culled
	bool anyInside = false;
	for( int currCamera = cam_StartIteration( ); ( currCamera != -1 ) && !anyInside; currCamera = cam_GetNextActiveCam( ) ) {
		if( cam_GetFlags( currCamera ) & camFlags ) {
			Matrix4 vpMat;
			cam_GetVPMatrix( currCamera, &vpMat );

			Vector2 p0, p1, p2;
			mat4_TransformVec2Pos( &vpMat, &( vert0.pos ), &p0 );
			mat4_TransformVec2Pos( &vpMat, &( vert1.pos ), &p1 );
			mat4_TransformVec2Pos( &vpMat, &( vert2.pos ), &p2 );
			anyInside = testTriangle( &p0, &p1, &p2 );
		}
	}
	if( !anyInside ) return 0;

	if( triList->lastTriIndex >= ( triList->triCount - 1 ) ) {
		llog( LOG_VERBOSE, "Triangle list full." );
		return -1;
	}

    //float baseZ = (float)depth - (float)INT8_MIN;
    //float z = baseZ + ( Z_ORDER_OFFSET * ( solidTriangles.lastTriIndex + transparentTriangles.lastTriIndex + 2 ) );
	float z = (float)depth + ( Z_ORDER_OFFSET * ( solidTriangles.lastTriIndex + transparentTriangles.lastTriIndex + 2 ) );
    
	int idx = triList->lastTriIndex + 1;
	triList->lastTriIndex = idx;
	triList->triangles[idx].camFlags = camFlags;
	triList->triangles[idx].texture = texture;
	triList->triangles[idx].zPos = z;
	triList->triangles[idx].shaderType = shader;
	triList->triangles[idx].stencilGroup = clippingID;
	triList->triangles[idx].floatVal0 = floatVal0;
	triList->triangles[idx].extraTexture = extraTexture;
	int baseIdx = idx * 3;

#define ADD_VERT( v, offset ) \
	vec2ToVec3( &( (v).pos ), z, &( triList->vertices[baseIdx + (offset)].pos ) ); \
	triList->vertices[baseIdx + (offset)].col = (v).col; \
	triList->vertices[baseIdx + (offset)].uv = (v).uv; \
	triList->triangles[idx].vertexIndices[(offset)] = baseIdx + (offset);

	ADD_VERT( vert0, 0 );
	ADD_VERT( vert1, 1 );
	ADD_VERT( vert2, 2 );

#undef ADD_VERT

	return 0;
}

/*
We'll assume the array has three vertices in it.
 Return a value < 0 if there's a problem.
*/
int triRenderer_AddVertices( TriVert* verts, ShaderType shader, PlatformTexture texture, PlatformTexture extraTexture,
	float floatVal0, int clippingID, uint32_t camFlags, int8_t depth, TriType type )
{
	return triRenderer_Add( verts[0], verts[1], verts[2], shader, texture, extraTexture, floatVal0, clippingID, camFlags, depth, type );
}

int triRenderer_Add( TriVert vert0, TriVert vert1, TriVert vert2, ShaderType shader, PlatformTexture texture, PlatformTexture extraTexture,
	float floatVal0, int clippingID, uint32_t camFlags, int8_t depth, TriType type )
{
	switch( type ) {
	case TT_SOLID:
		return addTriangle( &solidTriangles, vert0, vert1, vert2, shader, texture, extraTexture, floatVal0, clippingID, camFlags, depth );
	case TT_TRANSPARENT:
		return addTriangle( &transparentTriangles, vert0, vert1, vert2, shader, texture, extraTexture, floatVal0, clippingID, camFlags, depth );
	case TT_STENCIL:
		return addTriangle( &stencilTriangles, vert0, vert1, vert2, shader, texture, extraTexture, floatVal0, clippingID, camFlags, depth );
	}
	return 0;
}

/*
Clears out all the triangles currently stored.
*/
void triRenderer_Clear( void )
{
	transparentTriangles.lastTriIndex = -1;
	solidTriangles.lastTriIndex = -1;
	stencilTriangles.lastTriIndex = -1;
}

static int sortByRenderState( const void* p1, const void* p2 )
{
	Triangle* tri1 = (Triangle*)p1;
	Triangle* tri2 = (Triangle*)p2;

	int stDiff = ( (int)tri1->shaderType - (int)tri2->shaderType );
	if( stDiff != 0 ) {
		return stDiff;
	}

	int comp = gfxPlatform_ComparePlatformTextures( tri1->texture, tri2->texture );
	if( comp != 0 ) {
		return comp;
	}

	return ( tri1->stencilGroup - tri2->stencilGroup );
}

static int sortByDepth( const void* p1, const void* p2 )
{
	Triangle* tri1 = (Triangle*)p1;
	Triangle* tri2 = (Triangle*)p2;
	return ( ( ( tri1->zPos ) - ( tri2->zPos ) ) > 0.0f ) ? 1 : -1;
}

/*
Draws out all the triangles.
*/
void triRenderer_Render( )
{
	// SDL_qsort appears to break some times, so fall back onto the standard library qsort for right now, and implement our own when we have time
	qsort( solidTriangles.triangles, solidTriangles.lastTriIndex + 1, sizeof( Triangle ), sortByRenderState );
	qsort( transparentTriangles.triangles, transparentTriangles.lastTriIndex + 1, sizeof( Triangle ), sortByDepth );
	qsort( stencilTriangles.triangles, stencilTriangles.lastTriIndex + 1, sizeof( Triangle ), sortByRenderState );

	triPlatform_RenderStart( &solidTriangles, &transparentTriangles, &stencilTriangles );

	// render triangles
	// TODO: We're ignoring any issues with cameras and transparency, probably want to handle this better.
	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		triPlatform_RenderForCamera( currCamera, &solidTriangles, &transparentTriangles, &stencilTriangles );
	}

	triPlatform_RenderEnd( );
}
