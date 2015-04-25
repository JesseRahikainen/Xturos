#ifndef TRI_RENDERING_H
#define TRI_RENDERING_H

#include <SDL_opengl.h>

#include "../Math/vector2.h"
#include "color.h"

void triRndr_Init( void );

/*
We'll assume the array has three vertices in it.
*/
int triRndr_Add( Vector2 positions, Vector2 uvs, GLuint texture, Color color, int camFlags, char depth );

void triRndr_DrawTriangles( int camera );

#endif /* inclusion guard */