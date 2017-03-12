#ifndef SCISSOR_H
#define SCISSOR_H

#include "../Graphics/glPlatform.h"

#include "../Math/vector2.h"

/*
Manages the scissor commmands we want to use so we can have different types of rendering use the
 same system.
*/

int scissor_Init( int renderWidth, int renderHeight );

void scissor_Clear( void );
int scissor_Push( const Vector2* upperLeft, const Vector2* size );
int scissor_Pop( void );

int scissor_GetTopID( void );
int scissor_GetScissorAreaGL( int id, GLint* outX, GLint* outY, GLsizei* outW, GLsizei* outH );
int scissor_GetScissorArea( int id, Vector2* outUpperLeft, Vector2* outSize );

#endif