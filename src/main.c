#include <stdio.h>
#include <stdlib.h>
#include <SDL_main.h>
#include <SDL.h>
#include <assert.h>

#include <time.h>

#include <float.h>
#include <math.h>

#include "Math/Vector2.h"
#include "Graphics/camera.h"
#include "Graphics/graphics.h"
#include "Math/MathUtil.h"
#include "sound.h"
#include "Utils/cfgFile.h"

#include "UI/text.h"
#include "Input/input.h"

#include "gameState.h"
#include "Game/gameScreen.h"

#include "System/memory.h"
#include "System/systems.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define RENDER_WIDTH 600
#define RENDER_HEIGHT 800

static int running;
static int focused;
static unsigned int lastTicks;
static SDL_Window* window;
static SDL_RWops* logFile;
static const char* windowName = "TESTING STUFF";

/* making PHYSICS_TICK to something that will result in a whole number should lead to better results
the second number is how many times per second it will update */
//#define PHYSICS_TICK ( 1000 / 5 )
#define PHYSICS_TICK ( 1000 / 60 )
#define PHYSICS_DELTA ( (float)PHYSICS_TICK / 1000.0f )

void cleanUp( void )
{
	SDL_DestroyWindow( window );
	window = NULL;

	shutDownMixer( );

	SDL_Quit( );

	if( logFile != NULL ) {
		SDL_RWclose( logFile );
	}

	mem_CleanUp( );

	atexit( NULL );
}

void LogOutput( void* userData, int category, SDL_LogPriority priority, const char* message )
{
	size_t strLen = SDL_strlen( message );
	SDL_RWwrite( logFile, message, 1, strLen );
#ifdef WIN32
	SDL_RWwrite( logFile, "\r\n", 1, 2 );
#else
	#warning "NO END OF LINE DEFINED FOR THIS PLATFORM!"
#endif
}

int initEverything( void )
{
#ifndef _DEBUG
	logFile = SDL_RWFromFile( "log.txt", "w" );
	if( logFile != NULL ) {
		SDL_LogSetOutputFunction( LogOutput, NULL );
	}
#endif

	// memory first, won't be used everywhere at first so lets keep the initial allocation low, 64 MB
	mem_Init( 64 * 1024 * 1024 );

	// then SDL
	SDL_SetMainReady( );
	if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS ) != 0 ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, SDL_GetError( ) );
		return -1;
	}
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "SDL successfully initialized" );
	atexit( cleanUp );

	// set up opengl
	//  try opening and parsing the config file
	int majorVersion;
	int minorVersion;
	int redSize;
	int greenSize;
	int blueSize;
	int depthSize;

	void* oglCFGFile = cfg_OpenFile( "opengl.cfg" );
	cfg_GetInt( oglCFGFile, "MAJOR", 3, &majorVersion );
	cfg_GetInt( oglCFGFile, "MINOR", 3, &minorVersion );
	cfg_GetInt( oglCFGFile, "RED_SIZE", 8, &redSize );
	cfg_GetInt( oglCFGFile, "GREEN_SIZE", 8, &greenSize );
	cfg_GetInt( oglCFGFile, "BLUE_SIZE", 8, &blueSize );
	cfg_GetInt( oglCFGFile, "DEPTH_SIZE", 16, &depthSize );
	cfg_CloseFile( oglCFGFile );

	// todo: commenting these out breaks the font rendering, something wrong with the texture that's created
	//  note: without these it uses the values loaded in from the .cfg file, which is 3.3
	majorVersion = 2;
	minorVersion = 1;
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, minorVersion );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, redSize );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, greenSize );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, blueSize );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, depthSize );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	window = SDL_CreateWindow( windowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
	if( window == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, SDL_GetError( ) );
		return -1;
	}
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "SDL OpenGL window successfully created" );

	// Create rendering
	if( gfx_Init( window, RENDER_WIDTH, RENDER_HEIGHT ) < 0 ) {
		return -1;
	}
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "Rendering successfully initialized" );

	// Create sound mixer
	if( initMixer( ) < 0 ) {
		return -1;
	}
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "Mixer successfully initialized" );

	cam_Init( );
	cam_SetProjectionMatrices( RENDER_WIDTH, RENDER_HEIGHT );
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "Cameras successfully initialized" );

	input_InitMouseInputArea( RENDER_WIDTH, RENDER_HEIGHT );
	input_UpdateMouseWindow( WINDOW_WIDTH, WINDOW_HEIGHT );
	SDL_LogInfo( SDL_LOG_CATEGORY_APPLICATION, "Input successfully initialize" );

	return 0;
}

/* input processing */
void processEvents( int windowsEventsOnly )
{
	SDL_Event e;
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
				focused = 1;
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				focused = 0;
				break;
			}
		}

		if( e.type == SDL_QUIT ) {
			running = 0;
		}

		if( windowsEventsOnly ) { 
			continue;
		}

		sys_ProcessEvents( &e );
		input_ProcessEvents( &e );
		gsmProcessEvents( &globalFSM, &e );
	}
}

int main( int argc, char** argv )
{
	unsigned int tickDelta;
	unsigned int currTicks;
	unsigned int physicsTickAcc;
	int numPhysicsProcesses;
	float renderDelta;

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
	running = 1;
	lastTicks = SDL_GetTicks( );
	physicsTickAcc = 0;

	gsmEnterState( &globalFSM, &gameScreenState );

	while( running ) {

		currTicks = SDL_GetTicks( );
		tickDelta = currTicks - lastTicks;
		lastTicks = currTicks;

		if( !focused ) {
			processEvents( 1 );
			continue;
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
		if( numPhysicsProcesses > 0 ) {
			// set the new render positions
			renderDelta = PHYSICS_DELTA * (float)numPhysicsProcesses;
			gfx_ClearDrawCommands( renderDelta );
			cam_FinalizeStates( renderDelta );

			// set up rendering for everything
			sys_Draw( );
			gsmDraw( &globalFSM );
		}

		// do the actual drawing for this frame
		float dt = (float)tickDelta / 1000.0f;
		cam_Update( dt );
		gfx_Render( dt );
		// flip here so we don't have to store the window anywhere else
		SDL_GL_SwapWindow( window );
	}

	return 0;
}