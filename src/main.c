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

#include "UI/text.h"
#include "Input/input.h"

#include "gameState.h"
#include "Game/gameScreen.h"

#include "System/memory.h"
#include "System/systems.h"
#include "System/platformLog.h"

#include "Graphics\debugRendering.h"
#include "Graphics\glPlatform.h"

#define RENDER_WIDTH 800
#define RENDER_HEIGHT 600
#ifdef __EMSCRIPTEN__
	#define WINDOW_WIDTH RENDER_WIDTH
	#define WINDOW_HEIGHT RENDER_HEIGHT
#else
	#define WINDOW_WIDTH 800
	#define WINDOW_HEIGHT 600
#endif

static bool running;
static bool focused;
static unsigned int lastTicks;
static unsigned int physicsTickAcc;
static SDL_Window* window;
static SDL_RWops* logFile;
static const char* windowName = "Tiny Tactics";

/* making PHYSICS_TICK to something that will result in a whole number should lead to better results
the second number is how many times per second it will update */
//#define PHYSICS_TICK ( 1000 / 5 )
#define PHYSICS_TICK ( 1000 / 60 )
#define PHYSICS_DELTA ( (float)PHYSICS_TICK / 1000.0f )

void cleanUp( void )
{
	SDL_DestroyWindow( window );
	window = NULL;

	snd_CleanUp( );

	SDL_Quit( );

	if( logFile != NULL ) {
		SDL_RWclose( logFile );
	}

	mem_CleanUp( );

	atexit( NULL );
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
	int* is = malloc( sizeof( int ) * 100 );
	for( int i = 0; i < 100; ++i ) {
		is[i] = i;
	}

	char* p = (char*)is;
	int* intData = (int*)((char*)&( p[1] ) );
	int unalignedInt = *intData;
	return unalignedInt;
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
	mem_Init( 64 * 1024 * 1024 );

	// then SDL
	SDL_SetMainReady( );
	if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS ) != 0 ) {
		llog( LOG_ERROR, "%s", SDL_GetError( ) );
		return -1;
	}
	llog( LOG_INFO, "SDL successfully initialized." );
	atexit( cleanUp );

	// set up opengl
	//  try opening and parsing the config file
	int majorVersion;
	int minorVersion;
	int redSize;
	int greenSize;
	int blueSize;
	int depthSize;

	void* oglCFGFile;
#if defined( __EMSCRIPTEN__ )
	oglCFGFile = cfg_OpenFile( "webgl.cfg" );
#else
	oglCFGFile = cfg_OpenFile( "opengl.cfg" );
#endif
	cfg_GetInt( oglCFGFile, "MAJOR", 3, &majorVersion );
	printf( "major: %i\n", majorVersion );
	cfg_GetInt( oglCFGFile, "MINOR", 3, &minorVersion );
	printf( "minor: %i\n", minorVersion );
	cfg_GetInt( oglCFGFile, "RED_SIZE", 8, &redSize );
	cfg_GetInt( oglCFGFile, "GREEN_SIZE", 8, &greenSize );
	cfg_GetInt( oglCFGFile, "BLUE_SIZE", 8, &blueSize );
	cfg_GetInt( oglCFGFile, "DEPTH_SIZE", 16, &depthSize );
	cfg_CloseFile( oglCFGFile );

	// todo: commenting these out breaks the font rendering, something wrong with the texture that's created
	//  note: without these it uses the values loaded in from the .cfg file, which is 3.3
//#if defined( __ANDROID__ ) //|| defined( __IOS__ )
	// setup using OpenGLES
//#else
	// setup using OpenGL
	//majorVersion = 2;
	//minorVersion = 0;
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, PROFILE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, minorVersion );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, redSize );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, greenSize );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, blueSize );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, depthSize );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
//#endif

	window = SDL_CreateWindow( windowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
	if( window == NULL ) {
		llog( LOG_ERROR, "%s", SDL_GetError( ) );
		return -1;
	}
	llog( LOG_INFO, "SDL OpenGL window successfully created" );

	// Create rendering
	if( gfx_Init( window, RENDER_WIDTH, RENDER_HEIGHT ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Rendering successfully initialized" );

	// Create sound mixer
	if( snd_Init( 2 ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "Mixer successfully initialized" );

	cam_Init( );
	cam_SetProjectionMatrices( RENDER_WIDTH, RENDER_HEIGHT );
	llog( LOG_INFO, "Cameras successfully initialized" );

	input_InitMouseInputArea( RENDER_WIDTH, RENDER_HEIGHT );

	// on mobile devices we can't assume the width and height we've used to create the window
	//  are the current width and height
	int winWidth;
	int winHeight;
	SDL_GetWindowSize( window, &winWidth, &winHeight );
	input_UpdateMouseWindow( winWidth, winHeight );
	llog( LOG_INFO, "Input successfully initialized" );

	initIMGUI( &inGameIMGUI, true, RENDER_WIDTH, RENDER_HEIGHT );
	initIMGUI( &editorIMGUI, false, winWidth, winHeight );
	llog( LOG_INFO, "IMGUI successfully initialized" );

	txt_Init( );

	llog( LOG_INFO, "Resources successfully loaded" );

	return 0;
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
			case SDL_WINDOWEVENT_RESIZED:
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

		sys_ProcessEvents( &e );
		input_ProcessEvents( &e );
		gsmProcessEvents( &globalFSM, &e );
		//imgui_ProcessEvents( &e ); // just for TextInput events when text input is enabled
		nk_xu_handleEvent( &editorIMGUI, &e );
		nk_xu_handleEvent( &inGameIMGUI, &e );
	}

	nk_input_end( &( editorIMGUI.ctx ) );
	nk_input_end( &( inGameIMGUI.ctx ) );
}

// needed to be able to work in javascript
void mainLoop( void* v )
{
	unsigned int tickDelta;
	unsigned int currTicks;
	int numPhysicsProcesses;
	float renderDelta;

#if defined( __EMSCRIPTEN__ )
	if( !running ) {
		emscripten_cancel_main_loop( );
	}
#endif

	currTicks = SDL_GetTicks( );
	tickDelta = currTicks - lastTicks;
	lastTicks = currTicks;

	if( !focused ) {
		processEvents( 1 );
		return;
	}

	physicsTickAcc += tickDelta;

	// process input
	processEvents( 0 );

	// handle per frame update
	sys_Process( );
	gsmProcess( &globalFSM );

	// process movement, collision, and other things that require a delta time
	numPhysicsProcesses = 0;
	while( physicsTickAcc > PHYSICS_TICK ) {
		sys_PhysicsTick( PHYSICS_DELTA );
		gsmPhysicsTick( &globalFSM, PHYSICS_DELTA );
		physicsTickAcc -= PHYSICS_TICK;
		++numPhysicsProcesses;
	}

	// rendering
	editorIMGUI.clear = false;
	inGameIMGUI.clear = false;
	if( numPhysicsProcesses > 0 ) {
		// set the new render positions
		renderDelta = PHYSICS_DELTA * (float)numPhysicsProcesses;
		gfx_ClearDrawCommands( renderDelta );
		cam_FinalizeStates( renderDelta );

		// set up rendering for everything
		sys_Draw( );
		gsmDraw( &globalFSM );

		editorIMGUI.clear = true;
		inGameIMGUI.clear = true;
	}

	// do the actual drawing for this frame
	float dt = (float)tickDelta / 1000.0f;
	cam_Update( dt );
	gfx_Render( dt );
	// flip here so we don't have to store the window anywhere else
	SDL_GL_SwapWindow( window );
}

int main( int argc, char** argv )
{
#ifdef _DEBUG
	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_VERBOSE );
#else
	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_WARN );
#endif
	SDL_LogSetAllPriority( SDL_LOG_PRIORITY_VERBOSE );

	if( initEverything( ) < 0 ) {
		return 1;
	}

	srand( (unsigned int)time( NULL ) );

	//***** main loop *****
	running = true;
	lastTicks = SDL_GetTicks( );
	physicsTickAcc = 0;
#if defined( __ANDROID__ )
	focused = true;
#endif

	gsmEnterState( &globalFSM, &gameScreenState );

#if defined( __EMSCRIPTEN__ )
	emscripten_set_main_loop_arg( mainLoop, NULL, -1, 1 );
#else
	while( running ) {
		mainLoop( NULL );
	}
#endif

	return 0;
}