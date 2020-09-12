#ifndef TRI_RENDERING_H
#define TRI_RENDERING_H

#include <stdint.h>

#include "../Graphics/glPlatform.h"
#include "glPlatform.h"

#include "../Math/vector2.h"
#include "color.h"

typedef enum {
	ST_DEFAULT,
	ST_ALPHA_ONLY,
	ST_SIMPLE_SDF,
	ST_IMAGE_SDF,
	NUM_SHADERS
} ShaderType;

typedef enum {
	TT_SOLID,
	TT_TRANSPARENT,
	TT_STENCIL,
} TriType;

typedef struct {
	Vector2 pos;
	Vector2 uv;
	Color col;
} TriVert;

TriVert triVert( Vector2 pos, Vector2 uv, Color col );

/*
Makes all the shaders reload.
*/
int triRenderer_LoadShaders( void );

/*
Initializes all the stuff needed for rendering the triangles.
 Returns a value < 0 if there's a problem.
*/
int triRenderer_Init( int renderAreaWidth, int renderAreaHeight );

/*
We'll assume the array has three vertices in it.
 Return a value < 0 if there's a problem.
*/
int triRenderer_AddVertices( TriVert* verts, ShaderType shader, GLuint texture, float floatVal0,
	int clippingID, uint32_t camFlags, int8_t depth, TriType type );
int triRenderer_Add( TriVert vert0, TriVert vert1, TriVert vert2, ShaderType shader, GLuint texture, float floatVal0,
	int clippingID, uint32_t camFlags, int8_t depth, TriType type );

/*
Clears out all the triangles currently stored.
*/
void triRenderer_Clear( void );

/*
Draws out all the triangles.
*/
void triRenderer_Render( );

#endif /* inclusion guard */