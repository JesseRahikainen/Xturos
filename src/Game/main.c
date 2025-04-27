#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

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
#include "System/ECPS/ecps_trackedCallbacks.h"

#include "gameState.h"
#include "Editors/editorHub.h"
#include "Game/testECPSScreen.h"
#include "Game/testAStarScreen.h"
#include "Game/testJobQueueScreen.h"
#include "Game/testSoundsScreen.h"
#include "Game/testPointerResponseScreen.h"
#include "Game/testSteeringScreen.h"
#include "Game/bordersTestScreen.h"
#include "Game/hexTestScreen.h"
#include "Game/testBloomScreen.h"
#include "Game/gameOfUrScreen.h"
#include "Game/testMountingState.h"

#include "System/memory.h"
#include "System/systems.h"
#include "System/platformLog.h"
#include "System/random.h"
#include "System/luaInterface.h"

#include "Graphics/debugRendering.h"
#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/gfxUtil.h"

#include "System/jobQueue.h"
#include "Utils/helpers.h"
#include "DefaultECPS/defaultECPS.h"

#define DESIRED_WORLD_WIDTH 800
#define DESIRED_WORLD_HEIGHT 600

#define DESIRED_RENDER_WIDTH 800
#define DESIRED_RENDER_HEIGHT 600
#ifdef __EMSCRIPTEN__
	#define DESIRED_WINDOW_WIDTH DESIRED_WORLD_WIDTH
	#define DESIRED_WINDOW_HEIGHT DESIRED_WORLD_WIDTH
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
static SDL_IOStream* logFile;
static const char* windowName = "Xturos";
static bool canResize;
static bool isEditorMode;

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

float getWindowRefreshRate( SDL_Window* w )
{
	const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode( w );
	if( mode == NULL ) {
		return DEFAULT_REFRESH_RATE;
	}

	if( mode->refresh_rate == 0 ) {
		return DEFAULT_REFRESH_RATE;
	}

	//llog( LOG_DEBUG, "w: %i  h: %i  r: %i", mode.w, mode.h, mode.refresh_rate );

	//int ww, wh;
	//SDL_GetWindowSize( w, &ww, &wh );
	//llog( LOG_DEBUG, "ww: %i  wh: %i", ww, wh );

	return mode->refresh_rate;
}

void cleanUp( void )
{
	xLua_ShutDown( );
	jq_ShutDown( );
	gfx_ShutDown( );
	snd_CleanUp( );
	if( logFile != NULL ) {
		SDL_CloseIO( logFile );
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
	SDL_WriteIO( logFile, message, strLen );
#if defined( WIN32 )
	SDL_WriteIO( logFile, "\r\n", 2 );
#elif defined( __ANDROID__ )
	SDL_WriteIO( logFile, "\n", 1 );
#elif defined( __EMSCRIPTEN__ )
	SDL_WriteIO( logFile, "\n", 1 );
#elif defined( __IPHONEOS__ )
	SDL_WriteIO( logFile, "\n", 1 );
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
	struct nk_font *font = nk_font_atlas_add_from_file( fontAtlas, "./Fonts/Aileron-Regular.otf", 14, 0 );
	
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
	// compile checks, we're assuming these types will have these sizes for serialization
	BUILD_BUG_ON( sizeof( float ) != 4 );
	BUILD_BUG_ON( sizeof( double ) != 8 );

#if defined( WIN32 )
	bool memValid = mem_Init( 256 * 1024 * 1024 );
#else
	bool memValid = mem_Init( 64 * 1024 * 1024 );
#endif
	if( !memValid ) {
		return -1;
	}

	SDL_SetMemoryFunctions( mem_AllocateForCallback, mem_ClearAllocateForCallback, mem_ResizeForCallback, mem_ReleaseForCallback );

#ifdef _DEBUG
	SDL_SetLogPriorities( SDL_LOG_PRIORITY_VERBOSE );
#else
	SDL_SetLogPriorities( SDL_LOG_PRIORITY_WARN );
#endif

#ifndef _DEBUG
	logFile = SDL_IOFromFile( "log.txt", "w" );
	if( logFile != NULL ) {
		SDL_SetLogOutputFunction( logOutput, NULL );
	}
#endif

#ifdef _DEBUG
	ecps_VerifyCallbackIDs( );
#endif

#if defined( __IPHONEOS__ )
    SDL_SetHint( SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2" );
#endif
    
	// then SDL
	SDL_SetMainReady( );
	if( !SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS ) ) {
		llog( LOG_ERROR, "Init Error: %s", SDL_GetError( ) );
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
	majorVersion = 3;
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
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 16 );
#endif

	llog( LOG_INFO, "Desired GL version: %i.%i", majorVersion, minorVersion );

 #if defined( __ANDROID__ ) || defined( __IPHONEOS__ )
	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( 0, &mode );
	int windowWidth = mode.w;
	int windowHeight = mode.h;
 #else
	int windowHeight = DESIRED_WINDOW_HEIGHT;
	int windowWidth = DESIRED_WINDOW_WIDTH;

	Uint32 windowFlags = SDL_WINDOW_OPENGL;

	if( canResize ) {
		windowFlags |= SDL_WINDOW_RESIZABLE;
	}
 #endif

#elif defined( METAL_GFX )
    
    Uint32 windowFlags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_METAL;

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

	window = SDL_CreateWindow( windowName, windowWidth, windowHeight, windowFlags );
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

	if( !xLua_Init( ) ) {
		llog( LOG_ERROR, "Unable to intialize Lua" );
		return -1;
	}
	llog( LOG_INFO, "Lua initialized" );

	defaultECPS_Setup( );

	input_ClearAllKeyBinds( );
	input_ClearAllMouseButtonBinds( );
	input_ClearAllSwipeBinds( );

	return 0;
}

int handleIOSEvents( void* userData, SDL_Event* event )
{
    switch( event->type ) {
    case SDL_EVENT_TERMINATING:
        /* Terminate the app.
           Shut everything down before returning from this function.
        */
        // we never seem to actually enter here
        cleanUp( );
        return 0;
    case SDL_EVENT_LOW_MEMORY:
        /* You will get this when your app is paused and iOS wants more memory.
           Release as much memory as possible.
        */
        return 0;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        /* Prepare your app to go into the background.  Stop loops, etc.
           This gets called when the user hits the home button, or gets a call.
        */
        // handled by the window lose focus event
        //focused = false;
        //snd_SetFocus( false );
        return 0;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        /* This will get called if the user accepted whatever sent your app to the background.
           If the user got a phone call and canceled it, you'll instead get an SDL_APP_DIDENTERFOREGROUND event and restart your loops.
           When you get this, you have 5 seconds to save all your state or the app will be terminated.
           Your app is NOT active at this point.
        */
        return 0;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
        /* This call happens when your app is coming back to the foreground.
           Restore all your state here.
        */
        return 0;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
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
		if( ( e.type >= SDL_EVENT_WINDOW_FIRST ) && ( e.type <= SDL_EVENT_WINDOW_LAST ) ) {
			switch( e.type ) {
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				if( canResize ) {
					//llog( LOG_DEBUG, "Window size changed to %i x %i", e.window.data1, e.window.data2 );
					// resize the rendering based on the size of the new window
					gfx_RenderResize( window, e.window.data1, e.window.data2 );

					// adjust world size so everything base don that scales correctly
					int worldHeight = DESIRED_WORLD_HEIGHT;
					int worldWidth = (int)( worldHeight * (float)e.window.data1 / (float)e.window.data2 );
					world_SetSize( worldWidth, worldHeight );
					//recalculateSafeArea( );

					// adjust the cameras to work with the new render aspect ratio
					//setCameraMatrices( worldWidth, worldHeight );

					// need to signal to everything else that stuff had changed and stuff may need to be relayed out
					//mb_BroadcastMessage( RESIZE_UI_ELEMENTS, NULL );

					// force a draw so everything updates correctly
					if( paused ) {
						forceDraw = true;
					}
				}
				// data1 == width, data2 == height
				gfx_SetWindowSize( e.window.data1, e.window.data2 );
				input_UpdateMouseWindow( e.window.data1, e.window.data2 );

				break;

			// will want to handle these messages for pausing and unpausing the game when they lose focus
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				llog( LOG_DEBUG, "Gained focus" );
				focused = true;
				snd_SetFocus( true );
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				llog( LOG_DEBUG, "Lost focus" );
				focused = false;
				snd_SetFocus( false );
				break;
			}
		}

		if( e.type == SDL_EVENT_QUIT ) {
			llog( LOG_DEBUG, "Quitting" );
			running = false;
		}

		if( windowsEventsOnly ) {
			continue;
		}

		// special keys used only on the pc version, primarily for taking screenshots of scenes at different resolutions
#if defined( WIN32 )
		if( e.type == SDL_EVENT_KEY_DOWN ) {

#ifdef _DEBUG
			if( e.key.key == SDLK_PRINTSCREEN ) {
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
#endif

			if( e.key.key == SDLK_PAUSE ) {
				// pause the game
				paused = !paused;
			}


			// have the F# keys change the size of the window based on some predetermined values
			int pressedFKey = -1;
			if( e.key.key == SDLK_F1 ) pressedFKey = 0;
			if( e.key.key == SDLK_F2 ) pressedFKey = 1;
			if( e.key.key == SDLK_F3 ) pressedFKey = 2;
			if( e.key.key == SDLK_F4 ) pressedFKey = 3;
			if( e.key.key == SDLK_F5 ) pressedFKey = 4;
			if( e.key.key == SDLK_F6 ) pressedFKey = 5;
			if( e.key.key == SDLK_F7 ) pressedFKey = 6;
			if( e.key.key == SDLK_F8 ) pressedFKey = 7;
			if( e.key.key == SDLK_F9 ) pressedFKey = 8;
			if( e.key.key == SDLK_F10 ) pressedFKey = 9;
			if( e.key.key == SDLK_F11 ) pressedFKey = 10;
			if( e.key.key == SDLK_F12 ) pressedFKey = 11;
			if( e.key.key == SDLK_F13 ) pressedFKey = 12;
			if( e.key.key == SDLK_F14 ) pressedFKey = 13;
			if( e.key.key == SDLK_F15 ) pressedFKey = 14;
			if( e.key.key == SDLK_F16 ) pressedFKey = 15;
			if( e.key.key == SDLK_F17 ) pressedFKey = 16;
			if( e.key.key == SDLK_F18 ) pressedFKey = 17;
			if( e.key.key == SDLK_F19 ) pressedFKey = 18;
			if( e.key.key == SDLK_F20 ) pressedFKey = 19;
			if( e.key.key == SDLK_F21 ) pressedFKey = 20;
			if( e.key.key == SDLK_F22 ) pressedFKey = 21;
			if( e.key.key == SDLK_F23 ) pressedFKey = 22;
			if( e.key.key == SDLK_F24 ) pressedFKey = 23;

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

#if defined( PROFILING_ENABLED )
	float procTimerSec = 0.0f;
	float physicsTimerSec = 0.0f;
	float drawTimerSec = 0.0f;
	float mainJobsTimerSec = 0.0f;
	float renderTimerSec = 0.0f;
	float flipTimerSec = 0.0f;
#endif

#if defined( PROFILING_ENABLED )
	Uint64 mainTimer = gt_StartTimer( );
#endif
	{

#if defined( __EMSCRIPTEN__ )
		if( !running ) {
			emscripten_cancel_main_loop( );
		}
#endif

		currTicks = SDL_GetPerformanceCounter( );
		tickDelta = currTicks - lastTicks;
		lastTicks = currTicks;

#if defined( PROFILING_ENABLED )
		Uint64 procTimer = gt_StartTimer( );
#endif
		{
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
		}
#if defined( PROFILING_ENABLED )
		procTimerSec = gt_StopTimer( procTimer );
#endif

#if defined( PROFILING_ENABLED )
		Uint64 physicsTimer = gt_StartTimer( );
#endif
		{
			// process movement, collision, and other things that require a delta time
			numPhysicsProcesses = 0;
			while( physicsTickAcc > PHYSICS_TICK ) {
				if( numPhysicsProcesses == 0 ) {
					// before we do any actual physics update any data we're using to the new frame
					gfx_ClearDrawCommands( );
				}

				sys_PhysicsTick( PHYSICS_DT );
				gsm_PhysicsTick( &globalFSM, PHYSICS_DT );
				physicsTickAcc -= PHYSICS_TICK;
				++numPhysicsProcesses;
			}
		}
#if defined( PROFILING_ENABLED )
		physicsTimerSec = gt_StopTimer( physicsTimer );
#endif

		// TODO: Figure out all the bugs that will happen because of this
		//  as of right now it's only used for taking screenshots of the same
		//  scene at different resolutions, not meant for actual in play use
		if( forceDraw && numPhysicsProcesses == 0 ) {
			numPhysicsProcesses = 1;
		}

#if defined( PROFILING_ENABLED )
		Uint64 drawTimer = gt_StartTimer( );
#endif
		{
			// drawing
			if( numPhysicsProcesses > 0 ) {
				// set the new draw positions
				renderDelta = PHYSICS_DT * (float)numPhysicsProcesses;
				gfx_SetDrawEndTime( renderDelta );
				cam_FinalizeStates( renderDelta );

				// set up drawing for everything
				sys_Draw( );
				gsm_Draw( &globalFSM );
			}
		}
#if defined( PROFILING_ENABLED )
		drawTimerSec = gt_StopTimer( drawTimer );
#endif

#if defined( PROFILING_ENABLED )
		Uint64 mainJobsTimer = gt_StartTimer( );
#endif
		{
			// process all the jobs we need the main thread for, using this reduces the need for synchronization
			jq_ProcessMainThreadJobs( );
		}
#if defined( PROFILING_ENABLED )
		mainJobsTimerSec = gt_StopTimer( mainJobsTimer );
#endif

		float dt = 0.0f;
#if defined( PROFILING_ENABLED )
		Uint64 renderTimer = gt_StartTimer( );
#endif
		{
			// do the actual rendering for this frame
			dt = (float)tickDelta / (float)SDL_GetPerformanceFrequency( );
			gt_SetRenderTimeDelta( dt );
			cam_Update( dt );
			gfx_Render( dt );
		}
#if defined( PROFILING_ENABLED )
		renderTimerSec = gt_StopTimer( renderTimer );
#endif

#if defined( PROFILING_ENABLED )
		Uint64 flipTimer = gt_StartTimer( );
#endif
		{
			gfx_Swap( window );
		}
#if defined( PROFILING_ENABLED )
		flipTimerSec = gt_StopTimer( flipTimer );
#endif
	}
#if defined( PROFILING_ENABLED )
	float mainTimerSec = gt_StopTimer( mainTimer );

	int priority = LOG_INFO;
	if( mainTimerSec >= 0.04f ) {
		priority = LOG_WARN;
	}
	llog( priority, "%sframeTime: %f - proc: %f, physics: %f, draw: %f, mainJobs: %f, render: %f, flip: %f",
		priority == LOG_WARN ? "!!! " : "",
		mainTimerSec, procTimerSec, physicsTimerSec, drawTimerSec, mainJobsTimerSec, renderTimerSec, flipTimerSec );
#endif
}

int main( int argc, char** argv )
{
	isEditorMode = false;
	canResize = false;
	for( int i = 1; i < argc; ++i ) {
		if( SDL_strcmp( argv[i], "-e" ) == 0 ) {
			isEditorMode = true;
			canResize = true;
		}
	}

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

	GameState* startState = NULL;
	startState = &testPointerResponseScreenState;
	//startState = &gameScreenState;
	//startState = &testAStarScreenState;
	//startState = &testJobQueueScreenState;
	//startState = &testSoundsScreenState;
	//startState = &testSteeringScreenState;
	//startState = &bordersTestScreenState;
	//startState = &hexTestScreenState;
	//startState = &testBloomScreenState;
	//startState = &gameOfUrScreenState;
	//startState = &testECPSScreenState;
	//startState = &testMountingState;
	if( isEditorMode ) {
		//startState = &editorHubScreenState;
	}
	gsm_EnterState( &globalFSM, startState );

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
