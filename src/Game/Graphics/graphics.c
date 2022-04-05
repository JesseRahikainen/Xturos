#include "graphics.h"

#include <SDL_syswm.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "camera.h"
#include "Math/mathUtil.h"
#include "Graphics/Platform/graphicsPlatform.h"

#include "images.h"
#include "debugRendering.h"
#include "spineGfx.h"
#include "triRendering.h"

#include "IMGUI/nuklearWrapper.h"

//#include "Graphics/Platform/OpenGL/glPlatform.h"

#include "System/platformLog.h"

#include "Utils/stretchyBuffer.h"

static float currentTime;
static float endTime;

// both the clear colors default to black
static Color gameClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
static Color windowClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

// defines the size of the actual area we'll be rendering to
static int renderWidth;
static int renderHeight;

// defines where in the window we'll copy the rendered area to
static int windowRenderX0;
static int windowRenderX1;
static int windowRenderY0;
static int windowRenderY1;

static GfxDrawTrisFunc* sbAdditionalDrawFuncs = NULL;
static GfxClearFunc* sbAdditionalClearFuncs = NULL;

void gfx_calculateRenderSize( int desiredRenderWidth, int desiredRenderHeight, int* outRenderWidth, int* outRenderHeight )
{
	int maxSize = gfxPlatform_GetMaxSize( MAX( renderWidth, renderHeight ) );

	renderWidth = desiredRenderWidth;
	renderHeight = desiredRenderHeight;

	float ratio = (float)renderWidth / (float)renderHeight;
	if( renderWidth > maxSize ) {
		renderWidth = (int)maxSize;
		renderHeight = (int)( maxSize / ratio );
		llog( LOG_DEBUG, "Render width outside maximum size allowed by OpenGL, resizing to %ix%i", renderWidth, renderHeight );
	} else if( renderHeight > maxSize ) {
		renderWidth = (int)( maxSize * ratio );
		renderHeight = (int)maxSize;
		llog( LOG_DEBUG, "Render height outside maximum size allowed by OpenGL, resizing to %ix%i", renderWidth, renderHeight );
	}
	(*outRenderWidth) = renderWidth;
	(*outRenderHeight) = renderHeight;
}

/*
Initial setup for the rendering instruction buffer.
 Returns 0 on success.
*/
int gfx_Init( SDL_Window* window, int desiredRenderWidth, int desiredRenderHeight )
{
	currentTime = 0.0f;
	endTime = 0.0f;

	if( !gfxPlatform_Init( window, desiredRenderWidth, desiredRenderHeight ) ) {
		return -1;
	}
	
	int windowWidth, windowHeight;
	SDL_GetWindowSize( window, &windowWidth, &windowHeight );
	llog( LOG_DEBUG, "Window size render: %i x %i", windowWidth, windowHeight );
    llog( LOG_DEBUG, "Desired size render: %i x %i", desiredRenderWidth, desiredRenderHeight );
	gfx_SetWindowSize( windowWidth, windowHeight );

	// initialize everything else
	if( img_Init( ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Images initialized." );

	if( debugRenderer_Init( ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Debug renderer initialized." );

	spine_Init( );
	llog( LOG_INFO, "Spine initialized." );

	if( triRenderer_Init( ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Triangle renderer initialized." );
    
	gameClearColor = CLR_MAGENTA;
    
#if defined( __IPHONEOS__ )
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( !SDL_GetWindowWMInfo( window, &info ) ) {
        llog( LOG_INFO, "Unable to retrieve window information: %s", SDL_GetError( ) );
        return -1;
    }
    
    if( info.subsystem != SDL_SYSWM_UIKIT ) {
        llog( LOG_INFO, "Invalid subsystem type." );
        return -1;
    }
    
    //defaultFBO = info.info.uikit.framebuffer;
    //defaultColorRBO = info.info.uikit.colorbuffer;
#endif

	return 0;
}

void gfx_ShutDown( void )
{
	gfxPlatform_ShutDown( );
}

void gfx_SetWindowSize( int windowWidth, int windowHeight )
{
	//llog( LOG_INFO, "Setting window size: %i x %i", windowWidth, windowHeight );
	int centerX = windowWidth / 2;
	int centerY = windowHeight / 2;

	int width;
	int height;

	float windowRatio = (float)windowWidth/(float)windowHeight;
	float renderRatio = (float)renderWidth/(float)renderHeight;

	if( windowRatio < renderRatio ) {
		windowRenderX0 = 0;
		windowRenderX1 = windowWidth;

		height = (int)( windowWidth / renderRatio );
		windowRenderY0 = centerY - ( height / 2 );
		windowRenderY1 = centerY + ( height / 2 );
	} else {
		width = (int)( windowHeight * renderRatio );
		windowRenderX0 = centerX - ( width / 2 );
		windowRenderX1 = centerX + ( width / 2 );

		windowRenderY0 = 0;
		windowRenderY1 = windowHeight;
	}
}

void gfx_RenderResize( SDL_Window* window, int newRenderWidth, int newRenderHeight )
{
	int finalWidth, finalHeight;
	gfx_calculateRenderSize( newRenderWidth, newRenderHeight, &finalWidth, &finalHeight );

	int windowWidth, windowHeight;
	SDL_GetWindowSize( window, &windowWidth, &windowHeight );
	gfx_SetWindowSize( windowWidth, windowHeight ); // adjusts the windowRenderDN values

	gfxPlatform_RenderResize( finalWidth, finalHeight );
}

void gfx_GetWindowSize( int* outWidth, int* outHeight )
{
	assert( outWidth != NULL );
	assert( outHeight != NULL );

	(*outWidth) = windowRenderX1 - windowRenderX0;
	(*outHeight) = windowRenderY1 - windowRenderY0;
}

/*
Just gets the size.
*/
void gfx_GetRenderSize( int* renderWidthOut, int* renderHeightOut )
{
	assert( renderWidthOut != NULL );
	assert( renderHeightOut != NULL );

	(*renderWidthOut) = renderWidth;
	(*renderHeightOut) = renderHeight;
}

void gfx_CleanUp( void )
{
	gfxPlatform_CleanUp( );
}

/*
Sets clearing color.
*/
void gfx_SetClearColor( Color newClearColor )
{
	gameClearColor = newClearColor;
}

/*
Sets the non render area clearing color.
*/
void gfx_SetWindowClearColor( Color newClearColor )
{
	windowClearColor = newClearColor;
}

/*
Clears all the drawing instructions.
*/
void gfx_ClearDrawCommands( float timeToEnd )
{
	debugRenderer_ClearVertices( );
	img_ClearDrawInstructions( );

	for( size_t i = 0; i < sb_Count( sbAdditionalClearFuncs ); ++i ) {
		sbAdditionalClearFuncs[i]( );
	}
	
	endTime = timeToEnd;
	currentTime = 0.0f;
}

void gfx_MakeRenderCalls( float dt, float t )
{
	spine_UpdateInstances( dt );
	spine_FlipInstancePositions( );

	// draw all the stuff that routes through the triangle rendering
	triRenderer_Clear( );
	img_Render( t );
	spine_RenderInstances( t );

	for( size_t i = 0; i < sb_Count( sbAdditionalDrawFuncs ); ++i ) {
		sbAdditionalDrawFuncs[i]( t );
	}
	triRenderer_Render( );

	// in game ui stuff
	//  note: this sets the glViewport, so if the render width and height of the imgui instance doesn't match the
	//   render width and height used above that will cause issues with the UI and the debug rendering
	//nk_xu_render( &inGameIMGUI );

	// now draw all the debug stuff over everything
	debugRenderer_Render( );
}

/*
Goes through everything in the render buffer and does the actual rendering.
Resets the render buffer.
*/
void gfx_Render( float dt )
{
	float t;
	currentTime += dt;
	t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );

#if defined( __EMSCRIPTEN__ )
	gfxPlatform_StaticSizeRender( dt, t, gameClearColor );
#else
	//gfxPlatform_StaticSizeRender( dt, t, windowClearColor );/*
	gfxPlatform_DynamicSizeRender( dt, t, renderWidth, renderHeight, windowRenderX0, windowRenderY0, windowRenderX1, windowRenderY1, gameClearColor );//*/
#endif
}

void gfx_AddDrawTrisFunc( GfxDrawTrisFunc newFunc )
{
	sb_Push( sbAdditionalDrawFuncs, newFunc );
}

void gfx_RemoveDrawTrisFunc( GfxDrawTrisFunc oldFunc )
{
	for( size_t i = 0; i < sb_Count( sbAdditionalDrawFuncs ); ++i ) {
		if( sbAdditionalDrawFuncs[i] == oldFunc ) {
			sb_Remove( sbAdditionalDrawFuncs, i );
			--i;
		}
	}
}

void gfx_AddClearCommand( GfxClearFunc newFunc )
{
	sb_Push( sbAdditionalClearFuncs, newFunc );
}

void gfx_RemoveClearCommand( GfxClearFunc oldFunc )
{
	for( size_t i = 0; i < sb_Count( sbAdditionalClearFuncs ); ++i ) {
		if( sbAdditionalClearFuncs[i] == oldFunc ) {
			sb_Remove( sbAdditionalClearFuncs, i );
			--i;
		}
	}
}

void gfx_Swap( SDL_Window* window )
{
    gfxPlatform_Swap( window );
}
