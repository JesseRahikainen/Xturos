/*
Simple button handling. Assume we're using a 3x3 image as the background (or really just 9 images), and some
text on the button itself. Will only handle a click event on release.
Need: Normal, focused, clicked; images, sounds, and functions
Don't manage resources used, expect them to be loaded somewhere else.
*/

#ifndef ENGINE_BUTTON_H
#define ENGINE_BUTTON_H

#include "Math/Vector2.h"
#include <SDL_events.h>

typedef void (*ButtonResponse)(void);

/* Call this before trying to use any buttons. */
void initButtons( );

/*
Creates a button. All image ids must be valid.
*/
int createButton( Vector2 position, Vector2 size, int normalImg, int focusedImg, int clickedImg,  unsigned int camFlags,
	char layer, ButtonResponse pressResponse, ButtonResponse releaseResponse );
void clearButton( int buttonIdx );
void clearAllButtons( );

void drawButtons( );
void processButtons( );
void processButtonEvents( SDL_Event* sdlEvent );

#endif
