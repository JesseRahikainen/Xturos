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
	NUM_SHADERS
} ShaderType;

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
int triRenderer_AddVertices( Vector2* positions, Vector2* uvs, ShaderType shader, GLuint texture, Color color,
	int clippingID, uint32_t camFlags, int8_t depth, int transparent );
int triRenderer_Add( Vector2 pos0, Vector2 pos1, Vector2 pos2, Vector2 uv0, Vector2 uv1, Vector2 uv2, ShaderType shader, GLuint texture,
	Color color, int clippingID, uint32_t camFlags, int8_t depth, int transparent );

/*
Clears out all the triangles currently stored.
*/
void triRenderer_Clear( void );

/*
Draws out all the triangles.
*/
void triRenderer_Render( );

#endif /* inclusion guard */