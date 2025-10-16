#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include <SDL3/SDL.h>
#include "Math/vector2.h"
#include "color.h"

typedef enum {
	RRT_MATCH, // the render area will be the same size as the window
	RRT_FIXED_HEIGHT, // the height of the render area wont' change but the width will be changed to fit the window, uses desired height
	RRT_FIXED_WIDTH, // the width of the render area won't change but the height will be changed to fit the window, uses desired width
	RRT_BEST_FIT, // based on the desired render area will resize fixing either the width or the height
	RRT_FIT_RENDER_INSIDE, // the render area will be a set size and fit completely inside the window
	RRT_FIT_RENDER_OUTSIDE, // the render area will be a set size and the window will fit inside it
	NUM_RENDER_RESIZE_TYPES
} RenderResizeType;

// t is in the range [0,1], where 0 is the start of the current draw cycle and 1 is the end
typedef void (*GfxDrawTrisFunc)( float t );
typedef void (*GfxClearFunc)( void );

// ======= Rendering =======
// Initial setup for the rendering instruction buffer.
//  Returns if it succeeded.
bool gfx_Init( SDL_Window* window, RenderResizeType type, int renderWidth, int renderHeight );

// Shuts down and cleans up
void gfx_ShutDown( void );

// the things we have are the window size, the render size, what sort of enveloping we want
void gfx_ChangeRenderResizeType( RenderResizeType type );
RenderResizeType gfx_GetRenderResizeType( void );

void gfx_SetDesiredRenderSize( int newDesiredWidth, int newDesiredHeight );
void gfx_GetRenderSize( int* outRenderWidth, int* outRenderHeight );

void gfx_OnWindowResize( int newWindowWidth, int newWindowHeight );
void gfx_GetWindowSize( int* outWindowWidth, int* outWindowHeight );

void gfx_BuildRenderTarget( void );

void gfx_GetRenderSubArea( int* outX0, int* outY0, int* outX1, int* outY1 );
void gfx_GetWindowSubArea( int* outX0, int* outY0, int* outX1, int* outY1 );

// Sets render area clearing color.
void gfx_SetClearColor( Color newClearColor );

// Sets the non render area clearing color.
void gfx_SetWindowClearColor( Color newClearColor );

// Clears all the drawing instructions.
void gfx_ClearDrawCommands( void );

void gfx_SetDrawEndTime( float timeToEnd );

// Goes through everything in the render buffer and does the actual rendering.
//  Resets the render buffer.
void gfx_Render( float deltaTime );

// Used to add additional calls to pass triangles to be rendered.
void gfx_AddDrawTrisFunc( GfxDrawTrisFunc newFunc );
void gfx_RemoveDrawTrisFunc( GfxDrawTrisFunc oldFunc );

void gfx_AddClearCommand( GfxClearFunc newFunc ); 
void gfx_RemoveClearCommand( GfxClearFunc oldFunc ); 

void gfx_calculateRenderSize( int desiredRenderWidth, int desiredRenderHeight, int* outRenderWidth, int* outRenderHeight );
void gfx_setRenderSize( int desiredRenderWidth, int desiredRenderHeight );
void gfx_MakeRenderCalls( float dt, float t );

// swap the buffers
void gfx_Swap( SDL_Window* window );

#endif
