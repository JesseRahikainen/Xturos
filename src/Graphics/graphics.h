#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include <SDL.h>
#include "../Math/vector2.h"
#include "color.h"

/* ======= Rendering ======= */
/*
Initial setup for the rendering instruction buffer.
 Returns a negative number on failure.
*/
int gfx_Init( SDL_Window* window );

/*
Sets clearing color.
*/
void gfx_SetClearColor( Color newClearColor );

/*
Clears all the drawing instructions.
*/
void gfx_ClearDrawCommands( float endTime );

/*
Goes through everything in the render buffer and does the actual rendering.
Resets the render buffer.
*/
void gfx_Render( float deltaTime );

#endif
