/*
Simple button handling. Assume we're using a 3x3 image as the background (or really just 9 images), and some
text on the button itself. Will only handle a click event on release.
Need: Normal, focused, clicked; images, sounds, and functions
Don't manage resources used, expect them to be loaded somewhere else.
*/

#ifndef ENGINE_BUTTON_H
#define ENGINE_BUTTON_H

#include "Math/vector2.h"
#include "Graphics/color.h"
#include <SDL_events.h>

typedef void (*ButtonResponse)(int);

/* Call this before trying to use any buttons. */
void btn_Init( );
void btn_CleanUp( );

int btn_Create( Vector2 position, Vector2 size, Vector2 clickedSize,
	const char* text, int fontID, float fontPixelSize, Color fontColor, Vector2 textOffset,
	int* slicedBorder, int imgID, Color imgColor,
	unsigned int camFlags, char layer, ButtonResponse pressResponse, ButtonResponse releaseResponse );
void btn_Destroy( int buttonIdx );
void btn_DestroyAll( void );

void btn_DebugDraw( Color idle, Color hover, Color clicked );

#endif
