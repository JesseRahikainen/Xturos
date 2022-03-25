#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <SDL_main.h>
#include <SDL.h>
#include <assert.h>

#include <time.h>

#include <float.h>
#include <math.h>

#include "Math/vector2.h"
#include "Graphics/camera.h"
#include "Graphics/graphics.h"
#include "Math/mathUtil.h"
#include "sound.h"
#include "Utils/cfgFile.h"
#include "IMGUI/nuklearWrapper.h"
#include "world.h"

#include "UI/text.h"
#include "Input/input.h"

#include "System/gameTime.h"

#include "gameState.h"
#include "Game/gameScreen.h"
#include "Game/testAStarScreen.h"
#include "Game/testJobQueueScreen.h"
#include "Game/testSoundsScreen.h"
#include "Game/testPointerResponseScreen.h"
#include "Game/testSteeringScreen.h"
#include "Game/bordersTestScreen.h"
#include "Game/hexTestScreen.h"
#include "Game/testBloomScreen.h"
#include "Game/gameOfUrScreen.h"

#include "System/memory.h"
#include "System/systems.h"
#include "System/platformLog.h"
#include "System/random.h"

#include "Graphics/debugRendering.h"
#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/gfxUtil.h"

#include "System/jobQueue.h"
#include "Utils/helpers.h"

// 540 x 960

#define DESIRED_WORLD_WIDTH 800
#define DESIRED_WORLD_HEIGHT 600

#define DESIRED_RENDER_WIDTH 800
#define DESIRED_RENDER_HEIGHT 600
#ifdef __EMSCRIPTEN__
	#define DESIRED_WINDOW_WIDTH RENDER_WIDTH
	#define DESIRED_WINDOW_HEIGHT RENDER_HEIGHT
#else
	#define DESIRED_WINDOW_WIDTH 800
	#define DESIRED_WINDOW_HEIGHT 600
#endif

#define DEFAULT_REFRESH_RATE 30

static bool running;
static bool focused;
static bool paused;
static bool forceDraw;
static Uint64 lastTicks;
static Uint64 physicsTickAcc;
static SDL_Window* window;
static SDL_RWops* logFile;
static const char* windowName = "Xturos";

typedef struct {
	uint32_t width;
	uint32_t height;
} AvailableResolution;

static AvailableResolution resolutions[] = {
	{ 540, 960 },
	{ 1242, 2688 },
	{ 1242, 2208 },
	{ 2048, 2732 },
	{ 300, 1000 },
	{ 2160, 3840 },
	{ 1080, 1920 },
};

int getWindowRefreshRate( SDL_Window* w )
{
	SDL_DisplayMode mode;
	if( SDL_GetWindowDisplayMode( w, &mode ) != 0 ) {
		return DEFAULT_REFRESH_RATE;
	}

	if( mode.refresh_rate == 0 ) {
		return DEFAULT_REFRESH_RATE;
	}

	//llog( LOG_DEBUG, "w: %i  h: %i  r: %i", mode.w, mode.h, mode.refresh_rate );

	//int ww, wh;
	//SDL_GetWindowSize( w, &ww, &wh );
	//llog( LOG_DEBUG, "ww: %i  wh: %i", ww, wh );

	return mode.refresh_rate;
}

void cleanUp( void )
{
	jq_ShutDown( );
	gfx_ShutDown( );
	snd_CleanUp( );
	if( logFile != NULL ) {
		SDL_RWclose( logFile );
	}
	mem_CleanUp( );
    
    SDL_DestroyWindow( window );
    window = NULL;

    SDL_Quit( );
}

// TODO: Move to logPlatform
void logOutput( void* userData, int category, SDL_LogPriority priority, const char* message )
{
	size_t strLen = SDL_strlen( message );
	SDL_RWwrite( logFile, message, 1, strLen );
#if defined( WIN32 )
	SDL_RWwrite( logFile, "\r\n", 1, 2 );
#elif defined( __ANDROID__ )
	SDL_RWwrite( logFile, "\n", 1, 1 );
#elif defined( __EMSCRIPTEN__ )
	SDL_RWwrite( logFile, "\n", 1, 1 );
#elif defined( __IPHONEOS__ )
    SDL_RWwrite( logFile, "\n", 1, 1 );
#else
	#warning "NO END OF LINE DEFINED FOR THIS PLATFORM!"
#endif
}

static void initIMGUI( NuklearWrapper* imgui, bool useRelativeMousePos, int width, int height )
{
	nk_xu_init( imgui, window, useRelativeMousePos, width, height );

	struct nk_font_atlas* fontAtlas;
	nk_xu_fontStashBegin( imgui, &fontAtlas );
	// load fonts
	struct nk_font *font = nk_font_atlas_add_from_file( fontAtlas, "./Fonts/kenpixel.ttf", 12, 0 );
	nk_xu_fontStashEnd( imgui );
	nk_style_set_font( &( imgui->ctx ), &( font->handle ) );
}

int unalignedAccess( void )
{
	llog( LOG_INFO, "Forcing unaligned access" );
#pragma warning( push )
#pragma warning( disable : 6011) // this is for testing behavior of a platform, never used in the rest of the program so it should cause no issues
	int* is = (int*)malloc( sizeof( int ) * 100 );

	for( int i = 0; i < 100; ++i ) {
		is[i] = i;
	}

	char* p = (char*)is;
	int* intData = (int*)((char*)&( p[1] ) );
	int unalignedInt = *intData;
	llog( LOG_INFO, "data: %i", *intData );

	intData = (int*)( (char*)&( p[2] ) );
	llog( LOG_INFO, "data: %i", *intData );

	intData = (int*)( (char*)&( p[3] ) );
	llog( LOG_INFO, "data: %i", *intData );

	return unalignedInt;
#pragma warning( pop )
}

int initEverything( void )
{
#ifndef _DEBUG
	logFile = SDL_RWFromFile( "log.txt", "w" );
	if( logFile != NULL ) {
		SDL_LogSetOutputFunction( logOutput, NULL );
	}
#endif

	//unalignedAccess( );

	llog( LOG_INFO, "Initializing memory." );
	// memory first, won't be used everywhere at first so lets keep the initial allocation low, 64 MB
#if defined( WIN32 )
	mem_Init( 256 * 1024 * 1024 );
#else
	mem_Init( 64 * 1024 * 1024 );
#endif

#if defined( __IPHONEOS__ )
    SDL_SetHint( SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2" );
#endif
    
	// then SDL
	SDL_SetMainReady( );
	if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS ) != 0 ) {
		llog( LOG_ERROR, "%s", SDL_GetError( ) );
		return -1;
	}
	llog( LOG_INFO, "SDL successfully initialized." );
    
#if !defined( __IPHONEOS__ )
	atexit( cleanUp );
#endif

#if defined( OPENGL_GFX )
	// set up opengl
	int majorVersion;
	int minorVersion;
	int redSize;
	int greenSize;
	int blueSize;
	int depthSize;
	int stencilSize;

 #if defined( __EMSCRIPTEN__ )
	majorVersion = 2;
	minorVersion = 0;
	redSize = 8;
	greenSize = 8;
	blueSize = 8;
	depthSize = 16;
	stencilSize = 8;
 #elif defined( __ANDROID__ ) || defined( __IPHONEOS__ )
	majorVersion = 3;
	minorVersion = 0;
	redSize = 8;
	greenSize = 8;
	blueSize = 8;
	depthSize = 16;
	stencilSize = 8;
 #else
	majorVersion = 3;
	minorVersion = 3;
	redSize = 8;
	greenSize = 8;
	blueSize = 8;
	depthSize = 16;
	stencilSize = 8;
 #endif

	// setup using OpenGL
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, PROFILE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, minorVersion );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, redSize );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, greenSize );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, blueSize );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, depthSize );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, stencilSize );
#if !defined( __ANDROID__ ) && !defined( __IPHONEOS__ )
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 4 );
#endif

	llog( LOG_INFO, "Desired GL version: %i.%i", majorVersion, minorVersion );

 #if defined( __ANDROID__ ) || defined( __IPHONEOS__ )
	Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( 0, &mode );
	int windowWidth = mode.w;
	int windowHeight = mode.h;
 #else
	int windowHeight = DESIRED_WINDOW_HEIGHT;
	int windowWidth = DESIRED_WINDOW_WIDTH;

	Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
 #endif

#elif defined( METAL_GFX )
    
    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_METAL;

    SDL_DisplayMode mode;
    SDL_GetDesktopDisplayMode( 0, &mode );
    
    int windowWidth = mode.w;
    int windowHeight = mode.h;
    
#endif
	//int renderHeight = DESIRED_RENDER_HEIGHT;
	//int renderWidth = (int)( renderHeight * (float)windowWidth / (float)windowHeight );
	int renderHeight = windowHeight;
	int renderWidth = windowWidth;

	int worldHeight = DESIRED_WORLD_HEIGHT;
	int worldWidth = (int)( worldHeight * (float)windowWidth / (float)windowHeight );

	//llog( LOG_INFO, "World size: %i x %i", worldWidth, worldHeight );
	world_SetSize( worldWidth, worldHeight );

	//llog( LOG_INFO, "Window size: %i x %i    Render size: %i x %i", windowWidth, windowHeight, renderWidth, renderHeight );

	window = SDL_CreateWindow( windowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		windowWidth, windowHeight, windowFlags );
	int finalWidth, finalHeight;
	SDL_GetWindowSize( window, &finalWidth, &finalHeight );
	//llog( LOG_INFO, "Final window size: %i x %i", finalWidth, finalHeight );

	if( window == NULL ) {
		llog( LOG_ERROR, "%s", SDL_GetError( ) );
		return -1;
	}
	llog( LOG_INFO, "SDL OpenGL window successfully created" );

	// Create rendering
	if( gfx_Init( window, renderWidth, renderHeight ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Rendering successfully initialized" );

	// Create sound mixer
	if( snd_Init( 2 ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Mixer successfully initialized" );

	cam_Init( );
	cam_SetProjectionMatrices( worldWidth, worldHeight, false );
	llog( LOG_INFO, "Cameras successfully initialized" );

	input_InitMouseInputArea( worldWidth, worldHeight );

	// on mobile devices we can't assume the width and height we've used to create the window
	//  are the current width and height
	int winWidth;
	int winHeight;
	SDL_GetWindowSize( window, &winWidth, &winHeight );
	input_UpdateMouseWindow( winWidth, winHeight );
	llog( LOG_INFO, "Input successfully initialized" );

	initIMGUI( &inGameIMGUI, true, renderWidth, renderHeight );
	initIMGUI( &editorIMGUI, false, winWidth, winHeight );
	llog( LOG_INFO, "IMGUI successfully initialized" );

	txt_Init( );

	rand_Seed( NULL, (uint32_t)time( NULL ) );

	jq_Initialize( 2 );
	return 0;
}

int handleIOSEvents( void* userData, SDL_Event* event )
{
    switch( event->type ) {
    case SDL_APP_TERMINATING:
        /* Terminate the app.
           Shut everything down before returning from this function.
        */
        // we never seem to actually enter here
        cleanUp( );
        return 0;
    case SDL_APP_LOWMEMORY:
        /* You will get this when your app is paused and iOS wants more memory.
           Release as much memory as possible.
        */
        return 0;
    case SDL_APP_WILLENTERBACKGROUND:
        /* Prepare your app to go into the background.  Stop loops, etc.
           This gets called when the user hits the home button, or gets a call.
        */
        // handled by the window lose focus event
        //focused = false;
        //snd_SetFocus( false );
        return 0;
    case SDL_APP_DIDENTERBACKGROUND:
        /* This will get called if the user accepted whatever sent your app to the background.
           If the user got a phone call and canceled it, you'll instead get an SDL_APP_DIDENTERFOREGROUND event and restart your loops.
           When you get this, you have 5 seconds to save all your state or the app will be terminated.
           Your app is NOT active at this point.
        */
        return 0;
    case SDL_APP_WILLENTERFOREGROUND:
        /* This call happens when your app is coming back to the foreground.
           Restore all your state here.
        */
        return 0;
    case SDL_APP_DIDENTERFOREGROUND:
        /* Restart your loops here.
           Your app is interactive and getting CPU again.
        */
        // handled by by the window gain focus event
        //focused = true;
        //snd_SetFocus( true );
        return 0;
    }
    
    return 1;
}

/* input processing */
void processEvents( int windowsEventsOnly )
{
	SDL_Event e;
	nk_input_begin( &( editorIMGUI.ctx ) );
	nk_input_begin( &( inGameIMGUI.ctx ) );
	while( SDL_PollEvent( &e ) != 0 ) {
		if( e.type == SDL_WINDOWEVENT ) {
			switch( e.window.event ) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
#ifdef RENDER_RESIZE
				// TODO: Make this based on a config file instead of a define
				// resize the rendering based on the size of the new window
				gfx_RenderResize( window, e.window.data1, e.window.data2 );

				// adjust world size so everything base don that scales correctly
				int worldHeight = DESIRED_WORLD_HEIGHT;
				int worldWidth = (int)( worldHeight * (float)e.window.data1 / (float)e.window.data2 );
				world_SetSize( worldWidth, worldHeight );
				recalculateSafeArea( );

				// adjust the cameras to work with the new render aspect ratio
				setCameraMatrices( worldWidth, worldHeight );

				// need to signal to everything else that stuff had changed and stuff may need to be relayed out
				mb_BroadcastMessage( RESIZE_UI_ELEMENTS, NULL );

				// force a draw so everything updates correctly
				if( paused ) {
					forceDraw = true;
				}
#endif
				// data1 == width, data2 == height
				gfx_SetWindowSize( e.window.data1, e.window.data2 );
				input_UpdateMouseWindow( e.window.data1, e.window.data2 );

				break;

			// will want to handle these messages for pausing and unpausing the game when they lose focus
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				llog( LOG_DEBUG, "Gained focus" );
				focused = true;
				snd_SetFocus( true );
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				llog( LOG_DEBUG, "Lost focus" );
				focused = false;
				snd_SetFocus( false );
				break;
			}
		}

		if( e.type == SDL_QUIT ) {
			llog( LOG_DEBUG, "Quitting" );
			running = false;
		}

		if( windowsEventsOnly ) {
			continue;
		}

		// special keys used only on the pc version, primarily for taking screenshots of scenes at different resolutions
#if defined( WIN32 )
		if( e.type == SDL_KEYDOWN ) {
			if( e.key.keysym.sym == SDLK_PRINTSCREEN ) {
				// take a screen shot

				time_t t = time( NULL );
				struct tm tm = *localtime( &t );

				// YYYYMMDDHHmmSS.png
#define FORMAT "%.4i%.2i%.2i%.2i%.2i%.2i.png"
				int size = snprintf( NULL, 0, FORMAT, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour + 1, tm.tm_min + 1, tm.tm_sec );
				char* fileName = mem_Allocate( size + 1 );
				snprintf( fileName, size + 1, FORMAT, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour + 1, tm.tm_min + 1, tm.tm_sec );
#undef FORMAT
				char* savePath = getSavePath( fileName );
				gfxUtil_TakeScreenShot( savePath );
				llog( LOG_DEBUG, "Screenshot taken %s", savePath );
				mem_Release( fileName );
				mem_Release( savePath );
			}

			if( e.key.keysym.sym == SDLK_PAUSE ) {
				// pause the game
				paused = !paused;
			}

			// have the F# keys change the size of the window based on some predetermined values
			int pressedFKey = -1;
			if( e.key.keysym.sym == SDLK_F1 ) pressedFKey = 0;
			if( e.key.keysym.sym == SDLK_F2 ) pressedFKey = 1;
			if( e.key.keysym.sym == SDLK_F3 ) pressedFKey = 2;
			if( e.key.keysym.sym == SDLK_F4 ) pressedFKey = 3;
			if( e.key.keysym.sym == SDLK_F5 ) pressedFKey = 4;
			if( e.key.keysym.sym == SDLK_F6 ) pressedFKey = 5;
			if( e.key.keysym.sym == SDLK_F7 ) pressedFKey = 6;
			if( e.key.keysym.sym == SDLK_F8 ) pressedFKey = 7;
			if( e.key.keysym.sym == SDLK_F9 ) pressedFKey = 8;
			if( e.key.keysym.sym == SDLK_F10 ) pressedFKey = 9;
			if( e.key.keysym.sym == SDLK_F11 ) pressedFKey = 10;
			if( e.key.keysym.sym == SDLK_F12 ) pressedFKey = 11;
			if( e.key.keysym.sym == SDLK_F13 ) pressedFKey = 12;
			if( e.key.keysym.sym == SDLK_F14 ) pressedFKey = 13;
			if( e.key.keysym.sym == SDLK_F15 ) pressedFKey = 14;
			if( e.key.keysym.sym == SDLK_F16 ) pressedFKey = 15;
			if( e.key.keysym.sym == SDLK_F17 ) pressedFKey = 16;
			if( e.key.keysym.sym == SDLK_F18 ) pressedFKey = 17;
			if( e.key.keysym.sym == SDLK_F19 ) pressedFKey = 18;
			if( e.key.keysym.sym == SDLK_F20 ) pressedFKey = 19;
			if( e.key.keysym.sym == SDLK_F21 ) pressedFKey = 20;
			if( e.key.keysym.sym == SDLK_F22 ) pressedFKey = 21;
			if( e.key.keysym.sym == SDLK_F23 ) pressedFKey = 22;
			if( e.key.keysym.sym == SDLK_F24 ) pressedFKey = 23;

			if( ( pressedFKey != -1 ) && ( pressedFKey < ARRAY_SIZE( resolutions ) ) ) {
				// switch resolution
				SDL_SetWindowSize( window, resolutions[pressedFKey].width, resolutions[pressedFKey].height );
			}
		}
#endif

		sys_ProcessEvents( &e );
		input_ProcessEvents( &e );
		gsm_ProcessEvents( &globalFSM, &e );
		//imgui_ProcessEvents( &e ); // just for TextInput events when text input is enabled
		nk_xu_handleEvent( &editorIMGUI, &e );
		nk_xu_handleEvent( &inGameIMGUI, &e );
	}

	nk_input_end( &( editorIMGUI.ctx ) );
	nk_input_end( &( inGameIMGUI.ctx ) );
}

static int skipEvents = 3;
// needed to be able to work in javascript
void mainLoop( void* v )
{
	Uint64 tickDelta;
	Uint64 currTicks;
	int numPhysicsProcesses;
	float renderDelta;

	Uint64 mainTimer = gt_StartTimer( );
    
#if defined( __EMSCRIPTEN__ )
	if( !running ) {
		emscripten_cancel_main_loop( );
	}
#endif

	currTicks = SDL_GetPerformanceCounter( );
	tickDelta = currTicks - lastTicks;
	lastTicks = currTicks;

	Uint64 procTimer = gt_StartTimer( );
	if( !focused ) {
		processEvents( 1 );
		return;
	}

	if( paused ) {
		tickDelta = 0;
	}

	physicsTickAcc += tickDelta;

	// process input
	if( skipEvents <= 0 ) {
		processEvents( 0 );
	} else {
		--skipEvents;
	}
    
	// handle per frame update
    sys_Process( );
    gsm_Process( &globalFSM );
	float procTimerSec = gt_StopTimer( procTimer );

	Uint64 physicsTimer = gt_StartTimer( );
	// process movement, collision, and other things that require a delta time
    numPhysicsProcesses = 0;
	while( physicsTickAcc > PHYSICS_TICK ) {
		sys_PhysicsTick( PHYSICS_DT );
		gsm_PhysicsTick( &globalFSM, PHYSICS_DT );
		physicsTickAcc -= PHYSICS_TICK;
		++numPhysicsProcesses;
	}
	float physicsTimerSec = gt_StopTimer( physicsTimer );

	// TODO: Figure out all the bugs that will happen because of this
	//  as of right now it's only used for taking screenshots of the same
	//  scene at different resolutions, not meant for actual in play use
	if( forceDraw && numPhysicsProcesses == 0 ) {
		numPhysicsProcesses = 1;
	}

	Uint64 drawTimer = gt_StartTimer( );
	// drawing
    if( numPhysicsProcesses > 0 ) {
		// set the new draw positions
		renderDelta = PHYSICS_DT * (float)numPhysicsProcesses;
		gfx_ClearDrawCommands( renderDelta );
		cam_FinalizeStates( renderDelta );

		// set up drawing for everything
		sys_Draw( );
		gsm_Draw( &globalFSM );
	}
	float drawTimerSec = gt_StopTimer( drawTimer );

	Uint64 mainJobsTimer = gt_StartTimer( );
	// process all the jobs we need the main thread for, using this reduces the need for synchronization
	jq_ProcessMainThreadJobs( );
	float mainJobsTimerSec = gt_StopTimer( mainJobsTimer );

	Uint64 renderTimer = gt_StartTimer( );
	// do the actual rendering for this frame
	float dt = (float)tickDelta / (float)SDL_GetPerformanceFrequency( ); //(float)tickDelta / 1000.0f;
	gt_SetRenderTimeDelta( dt );
	cam_Update( dt );
	gfx_Render( dt );
    
	float renderTimerSec = gt_StopTimer( renderTimer );
	//llog( LOG_DEBUG, "FPS: %f", ( 1.0f / dt ) );

	Uint64 flipTimer = gt_StartTimer( );
    
    gfx_Swap( window );
    
	//float flipTimerSec = gt_StopTimer( flipTimer );

/*	if( dt >= 0.02f ) {
		llog( LOG_INFO, "!!! dt: %f", dt );
	} else {
		llog( LOG_INFO, "dt: %f", dt );
	}//*/

	float mainTimerSec = gt_StopTimer( mainTimer );

	//int priority = ( mainTimerSec >= 0.15f ) ? LOG_WARN : LOG_INFO;
	//llog( priority, "%smain: %.4f - proc: %.4f  phys: %.4f  draw: %.4f  jobs: %.4f  rndr: %.4f  flip: %.4f", ( mainTimerSec >= 0.02f ) ? "!!! " : "", mainTimerSec, procTimerSec, physicsTimerSec, drawTimerSec, mainJobsTimerSec, renderTimerSec, flipTimerSec );
	//llog( priority, "%smain: %.4f", ( mainTimerSec >= 0.02f ) ? "!!! " : "", mainTimerSec );
}

#include "Utils/hashMap.h"
int main( int argc, char** argv )
{
/*#ifdef _DEBUG
	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_VERBOSE );
#else
	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_WARN );
#endif//*/

	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_VERBOSE );

	if( initEverything( ) < 0 ) {
		return 1;
	}
    
	srand( (unsigned int)time( NULL ) );

	//***** main loop *****
	running = true;
	paused = false;
	forceDraw = false;
	lastTicks = SDL_GetPerformanceCounter( );
	physicsTickAcc = 0;
#if defined( __ANDROID__ ) || defined( __IPHONEOS__ )
	focused = true;
#endif

	//gsm_EnterState( &globalFSM, &gameScreenState );
	//gsm_EnterState( &globalFSM, &testAStarScreenState );
	//gsm_EnterState( &globalFSM, &testJobQueueScreenState );
	//gsm_EnterState( &globalFSM, &testSoundsScreenState );
	//gsm_EnterState( &globalFSM, &testPointerResponseScreenState );
	//gsm_EnterState( &globalFSM, &testSteeringScreenState );
	//gsm_EnterState( &globalFSM, &bordersTestScreenState );
	//gsm_EnterState( &globalFSM, &hexTestScreenState );
	//gsm_EnterState( &globalFSM, &testBloomScreenState );
	gsm_EnterState( &globalFSM, &gameOfUrScreenState );

#if defined( __EMSCRIPTEN__ )
	emscripten_set_main_loop_arg( mainLoop, NULL, -1, 1 );
#elif defined( __IPHONEOS__ )
    SDL_SetEventFilter( handleIOSEvents, NULL );
    if( SDL_iPhoneSetAnimationCallback( window, 1, mainLoop, NULL ) < 0 ) {
        llog( LOG_ERROR, "Unable to set callback: %s", SDL_GetError( ) );
    }
#else
	while( running ) {
		mainLoop( NULL );
	}
#endif

	return 0;
}
