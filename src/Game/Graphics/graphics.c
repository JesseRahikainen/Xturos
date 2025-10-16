#include "graphics.h"

#include <stdlib.h>
#include <SDL3/SDL_assert.h>
#include <math.h>
#include <string.h>

#include "camera.h"
#include "Math/mathUtil.h"
#include "Graphics/Platform/graphicsPlatform.h"
#include "System/systems.h"
#include "gameState.h"
#include "System/gameTime.h"

#include "images.h"
#include "spriteAnimation.h"
#include "debugRendering.h"
#include "triRendering.h"

#include "Utils/helpers.h"

#include "IMGUI/nuklearWrapper.h"

#include "System/platformLog.h"

#include "Utils/stretchyBuffer.h"
#include "System/messageBroadcast.h"

static RenderResizeType renderResizeType;

static float currentTime;
static float endTime;

// both the clear colors default to black
static Color gameClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
static Color windowClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

// defines the size of the actual area we'll be rendering to
static int renderWidth;
static int renderHeight;

// when using the best fit resizing we need the base desired area to calculate the final area
static int desiredRenderWidth;
static int desiredRenderHeight;

// stored and used when we need to calculate things
static int windowWidth;
static int windowHeight;

// defines where in the window we'll copy the rendered area to
static int windowRenderX0;
static int windowRenderX1;
static int windowRenderY0;
static int windowRenderY1;

// defines the area we'll pull from the rendered area to copy from
static int renderRenderX0;
static int renderRenderX1;
static int renderRenderY0;
static int renderRenderY1;


static GfxDrawTrisFunc* sbAdditionalDrawFuncs = NULL;
static GfxClearFunc* sbAdditionalClearFuncs = NULL;



static void fitAreaInsideOtherSetOuter( int innerWidth, int innerHeight, int outerWidth, int outerHeight,
	int* outInnerX0, int* outInnerY0, int* outInnerX1, int* outInnerY1,
	int* outOuterX0, int* outOuterY0, int* outOuterX1, int* outOuterY1 )
{
	int centerX = outerWidth / 2;
	int centerY = outerHeight / 2;

	float outerRatio = (float)outerWidth / (float)outerHeight;
	float innerRatio = (float)innerWidth / (float)innerHeight;

	( *outInnerX0 ) = 0;
	( *outInnerY0 ) = 0;
	( *outInnerX1 ) = innerWidth;
	( *outInnerY1 ) = innerHeight;

	if( FLT_EQ( outerRatio, innerRatio ) ) {
		// if they're equal there's a chance of some precision errors causing issues, so just assign directly
		( *outOuterX0 ) = 0;
		( *outOuterY0 ) = 0;
		( *outOuterX1 ) = outerWidth;
		( *outOuterY1 ) = outerHeight;
	} else if( outerRatio < innerRatio ) {
		( *outOuterX0 ) = 0;
		( *outOuterX1 ) = outerWidth;

		int height = (int)( outerWidth / innerRatio );
		( *outOuterY0 ) = centerY - ( height / 2 );
		( *outOuterY1 ) = centerY + ( height / 2 );
	} else {
		int width = (int)( outerHeight * innerRatio );
		( *outOuterX0 ) = centerX - ( width / 2 );
		( *outOuterX1 ) = centerX + ( width / 2 );

		( *outOuterY0 ) = 0;
		( *outOuterY1 ) = outerHeight;
	}
}

static void recalculateRenderPositions( void )
{
	switch( renderResizeType ) {
	case RRT_MATCH:
	case RRT_FIXED_HEIGHT:
	case RRT_FIXED_WIDTH:
	case RRT_BEST_FIT:
		// all of these should render to the entire window
		windowRenderX0 = 0;
		windowRenderY0 = 0;
		windowRenderX1 = windowWidth;
		windowRenderY1 = windowHeight;

		renderRenderX0 = 0;
		renderRenderY0 = 0;
		renderRenderX1 = renderWidth;
		renderRenderY1 = renderHeight;
		break;
	case RRT_FIT_RENDER_INSIDE:
		fitAreaInsideOtherSetOuter(
			renderWidth, renderHeight, windowWidth, windowHeight,
			&renderRenderX0, &renderRenderY0, &renderRenderX1, &renderRenderY1,
			&windowRenderX0, &windowRenderY0, &windowRenderX1, &windowRenderY1 );
		break;
	case RRT_FIT_RENDER_OUTSIDE:
		fitAreaInsideOtherSetOuter(
			windowWidth, windowHeight, renderWidth, renderHeight,
			&windowRenderX0, &windowRenderY0, &windowRenderX1, &windowRenderY1,
			&renderRenderX0, &renderRenderY0, &renderRenderX1, &renderRenderY1 );
		break;
	default:
		ASSERT_ALWAYS( "No implemented case." );
	}
}

static void calculateRenderFixedWidth( int* outNewWidth, int* outNewHeight )
{
	( *outNewWidth ) = desiredRenderWidth;
	float ratio = (float)windowWidth / (float)windowHeight;
	( *outNewHeight ) = (int)( desiredRenderWidth / ratio );
}

static void calculateRenderFixedHeight( int* outNewWidth, int* outNewHeight )
{
	( *outNewHeight ) = desiredRenderHeight;
	float ratio = (float)windowWidth / (float)windowHeight;
	( *outNewWidth ) = (int)( desiredRenderHeight * ratio );
}

static void calculateRenderBestFit( int* outNewWidth, int* outNewHeight )
{
	// for best fit we'll check the window ratio against the desired render ratio and choose one
	float windowRatio = (float)windowWidth / (float)windowHeight;
	float renderRatio = (float)renderWidth / (float)renderHeight;

	if( FLT_EQ( windowRatio, renderRatio ) ) {
		// if they're equal there's a chance of some precision errors causing issues
		( *outNewWidth ) = windowWidth;
		( *outNewHeight ) = windowHeight;
	} else if( windowRatio < renderRatio ) {
		calculateRenderFixedWidth( outNewWidth, outNewHeight );
	} else {
		calculateRenderFixedHeight( outNewWidth, outNewHeight );
	}
}

static bool updateRenderDimensions( void )
{
	// we're assuming things have changed 
	int newRenderWidth = desiredRenderWidth;
	int newRenderHeight = desiredRenderHeight;

	// recalculate the actual render sizes based on the 
	switch( renderResizeType ) {
	case RRT_MATCH:
		// resize render to match window
		newRenderWidth = windowWidth;
		newRenderHeight = windowHeight;
		break;
	case RRT_FIXED_HEIGHT:
		calculateRenderFixedHeight( &newRenderWidth, &newRenderHeight );
		break;
	case RRT_FIXED_WIDTH:
		calculateRenderFixedWidth( &newRenderWidth, &newRenderHeight );
		break;
	case RRT_BEST_FIT:
		calculateRenderBestFit( &newRenderWidth, &newRenderHeight );
		break;

		// these two won't change the size of the renderer
	case RRT_FIT_RENDER_INSIDE:
	case RRT_FIT_RENDER_OUTSIDE:
		break;
	default:
		ASSERT_ALWAYS( "No implemented case." );
	}

	bool anyChange = false;

	if( ( newRenderWidth != renderWidth ) || ( newRenderHeight != renderHeight ) ) {
		renderWidth = newRenderWidth;
		renderHeight = newRenderHeight;
		anyChange = true;
	}
	recalculateRenderPositions( );

	return anyChange;
}

// Initial setup for the rendering instruction buffer.
//  Returns 0 on success.
bool gfx_Init( SDL_Window* window, RenderResizeType type, int initRenderWidth, int initRenderHeight )
{
	currentTime = 0.0f;
	endTime = 0.0f;

	SDL_GetWindowSize( window, &windowWidth, &windowHeight );
	llog( LOG_DEBUG, "Window size render: %i x %i", windowWidth, windowHeight );
	llog( LOG_DEBUG, "Desired size render: %i x %i", initRenderWidth, initRenderHeight );

	renderWidth = initRenderWidth;
	renderHeight = initRenderHeight;
	desiredRenderWidth = initRenderWidth;
	desiredRenderHeight = initRenderHeight;
	renderResizeType = type;
	updateRenderDimensions( );
	cam_SetProjectionMatrices( renderWidth, renderHeight );

	if( !gfxPlatform_Init( window, VS_ADAPTIVE ) ) {
		return false;
	}
	
	// initialize everything else
	if( !img_Init( ) ) {
		return false;
	}
	llog( LOG_INFO, "Images initialized." );

	if( debugRenderer_Init( ) < 0 ) {
		return false;
	}
	llog( LOG_INFO, "Debug renderer initialized." );

	if( triRenderer_Init( ) < 0 ) {
		return false;
	}
	llog( LOG_INFO, "Triangle renderer initialized." );

	if( !sprAnim_Init( ) ) {
		return false;
	}
	llog( LOG_INFO, "Sprite animations initialized." );
    
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
#endif

	return true;
}

void gfx_ShutDown( void )
{
	gfxPlatform_ShutDown( );
}

void gfx_SetDesiredRenderSize( int newDesiredWidth, int newDesiredHeight )
{
	ASSERT_AND_IF_NOT( newDesiredWidth > 0 ) return;
	ASSERT_AND_IF_NOT( newDesiredHeight > 0 ) return;

	// nothing to change
	if( ( desiredRenderWidth == newDesiredWidth ) && ( desiredRenderHeight == newDesiredHeight ) ) return;
	if( renderResizeType != RRT_BEST_FIT ) return;

	desiredRenderWidth = newDesiredWidth;
	desiredRenderHeight = newDesiredHeight;

	if( updateRenderDimensions( ) ) {
		gfx_BuildRenderTarget( );
	}
}

RenderResizeType gfx_GetRenderResizeType( void )
{
	return renderResizeType;
}

static void runChangeUpdates( void )
{
	gfx_BuildRenderTarget( );
	cam_SetProjectionMatrices( renderWidth, renderHeight );
}

void gfx_ChangeRenderResizeType( RenderResizeType type )
{
	if( renderResizeType == type ) return;

	renderResizeType = type;
	if( updateRenderDimensions( ) ) {
		runChangeUpdates( );
	}
	mb_BroadcastMessage( MSG_RENDER_RESIZED, NULL );
}

void gfx_OnWindowResize( int newWindowWidth, int newWindowHeight )
{
	ASSERT_AND_IF_NOT( newWindowWidth > 0 ) return;
	ASSERT_AND_IF_NOT( newWindowHeight > 0 ) return;

	windowWidth = newWindowWidth;
	windowHeight = newWindowHeight;

	if( updateRenderDimensions( ) ) {
		runChangeUpdates( );
	}
	mb_BroadcastMessage( MSG_RENDER_RESIZED, NULL );

	llog( LOG_DEBUG, "Resize - window: %ix%i  render: %ix%i", windowWidth, windowHeight, renderWidth, renderHeight );
}

void gfx_GetRenderSize( int* outRenderWidth, int* outRenderHeight )
{
	ASSERT_AND_IF_NOT( outRenderWidth != NULL ) return;
	ASSERT_AND_IF_NOT( outRenderHeight != NULL ) return;

	( *outRenderWidth ) = renderWidth;
	( *outRenderHeight ) = renderHeight;
}

void gfx_GetWindowSize( int* outWindowWidth, int* outWindowHeight )
{
	ASSERT_AND_IF_NOT( outWindowWidth != NULL ) return;
	ASSERT_AND_IF_NOT( outWindowHeight != NULL ) return;

	( *outWindowWidth ) = windowWidth;
	( *outWindowHeight ) = windowHeight;
}

void gfx_GetRenderSubArea( int* outX0, int* outY0, int* outX1, int* outY1 )
{
	ASSERT_AND_IF_NOT( outX0 != NULL ) return;
	ASSERT_AND_IF_NOT( outY0 != NULL ) return;
	ASSERT_AND_IF_NOT( outX1 != NULL ) return;
	ASSERT_AND_IF_NOT( outY1 != NULL ) return;

	( *outX0 ) = renderRenderX0;
	( *outY0 ) = renderRenderY0;
	( *outX1 ) = renderRenderX1;
	( *outY1 ) = renderRenderY1;
}

void gfx_GetWindowSubArea( int* outX0, int* outY0, int* outX1, int* outY1 )
{
	ASSERT_AND_IF_NOT( outX0 != NULL ) return;
	ASSERT_AND_IF_NOT( outY0 != NULL ) return;
	ASSERT_AND_IF_NOT( outX1 != NULL ) return;
	ASSERT_AND_IF_NOT( outY1 != NULL ) return;

	( *outX0 ) = windowRenderX0;
	( *outY0 ) = windowRenderY0;
	( *outX1 ) = windowRenderX1;
	( *outY1 ) = windowRenderY1;
}

void gfx_BuildRenderTarget( void )
{
	gfxPlatform_RenderResize( renderWidth, renderHeight );
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
void gfx_ClearDrawCommands( )
{
	debugRenderer_ClearVertices( );

	for( size_t i = 0; i < sb_Count( sbAdditionalClearFuncs ); ++i ) {
		sbAdditionalClearFuncs[i]( );
	}
}

void gfx_SetDrawEndTime( float timeToEnd )
{
	endTime = timeToEnd;
	currentTime = 0.0f;
}

void gfx_MakeRenderCalls( float dt, float t )
{
	// draw all the stuff that routes through the triangle rendering
	triRenderer_Clear( );
	sys_Render( t );
	gsm_Render( &globalFSM, t );

	for( size_t i = 0; i < sb_Count( sbAdditionalDrawFuncs ); ++i ) {
		sbAdditionalDrawFuncs[i]( t );
	}
	triRenderer_Render( );

	// in game ui stuff
	//  note: this sets the glViewport, so if the render width and height of the imgui instance doesn't match the
	//   render width and height used above that will cause issues with the UI and the debug rendering
	nk_xu_render( &inGameIMGUI );

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
	gt_SetRenderNormalizedTime( t );

#if defined( __EMSCRIPTEN__ )
	gfxPlatform_StaticSizeRender( dt, t, gameClearColor );
#else
	gfxPlatform_DynamicSizeRender( dt, t,
		renderRenderX0, renderRenderY0, renderRenderX1, renderRenderY1,
		windowRenderX0, windowRenderY0, windowRenderX1, windowRenderY1,
		gameClearColor, windowClearColor );
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
