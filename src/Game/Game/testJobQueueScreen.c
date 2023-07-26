#include "testJobQueueScreen.h"

#include <stdbool.h>

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "UI/text.h"
#include "Utils/helpers.h"
#include "Input/input.h"

#include "System/platformLog.h"

#include "UI/button.h"

#include "System/jobQueue.h"

#include "sound.h"

static int font;
static int whiteImg;

static int testImg;
static int testFont;

static int testSound;

// baseic producer/consumer setup to allow
static bool prodConsActive = false;
static SDL_mutex* bufferLock = NULL;
static SDL_cond* canProduce = NULL;
static SDL_cond* canConsume = NULL;
static int buffer = -1;

static void delayedLoadTest( void* data )
{
	// to test the delay
	SDL_Delay( 1000 );
	img_ThreadedLoad( "Images/tile.png", ST_DEFAULT, &testImg, NULL );
}

static void delayedFontLoadTest( void* data )
{
	SDL_Delay( 2000 );
	txt_ThreadedLoadFont( "Fonts/Aileron-Regular.otf", 24.0f, &testFont );
}

static void delayedSoundLoadTest( void* data )
{
	SDL_Delay( 1500 );
	snd_ThreadedLoadSample( "Sounds/test.ogg", 1, false, &testSound, NULL );
}

#include <stdio.h>
static void testJob( void* data )
{
	for( int i = 0; i < 10; ++i ) {
		// NOTE: SDL logging is NOT thread safe, so this might cause a crash
		printf( "Job %i\n", i );
		SDL_Delay( 1000 ); // wait one second
	}
}

static void testJobTwo( void* data )
{
	for( int i = 0; i < 10; ++i ) {
		// NOTE: SDL logging is NOT thread safe, so this might cause a crash
		printf( "Fast Job %i\n", i );
		SDL_Delay( 500 ); // wait half a second
	}
}

static void addTenJobs( void* data )
{
	for( int i = 0; i < 10; ++i ) {
		if( i % 2 == 0 ) {
			jq_AddJob( testJob, NULL );
		} else {
			jq_AddJob( testJobTwo, NULL );
		}
		SDL_Delay( 10 );
	}
}

static void addHundredJobs( void* data )
{
	for( int i = 0; i < 100; ++i ) {
		jq_AddJob( testJob, NULL );
		SDL_Delay( 10 );
	}
}

static void createJob( int btnID )
{
	jq_AddJob( testJob, NULL );
}

static void createTenJobs( int btnID )
{
	//jq_AddJob( addTenJobs, NULL );
	addTenJobs( NULL );
}

static void createHundredJobs( int btnID )
{
	//jq_AddJob( addHundredJobs, NULL );
	addHundredJobs( NULL );
}

static void createDelayedLoadTest( int btnID )
{
	jq_AddJob( delayedLoadTest, NULL );
}

static void createDelayedFontLoadTest( int btnID )
{
	jq_AddJob( delayedFontLoadTest, NULL );
}

static void createDelayedSoundLoadTest( int btnID )
{
	jq_AddJob( delayedSoundLoadTest, NULL );
}

static void testSoundPlay( int btnID )
{
	if( testSound >= 0 ) {
		snd_Play( testSound, 0.5f, 1.0f, 0.0f, 0 );
	}
}

static void testProducer( void* data )
{
	printf( "Producer started\n" );

	for( int i = 0; i < 100; ++i ) {
		SDL_Delay( rand( ) % 1000 );

		SDL_LockMutex( bufferLock ); {
			if( buffer != -1 ) {
				printf( "Producer encountered full buffer, waiting for consumer to empty it.\n" );
				SDL_CondWait( canProduce, bufferLock );
			}

			buffer = rand( ) % 255;
			printf( "Produced %i\n", buffer );
		} SDL_UnlockMutex( bufferLock );
		SDL_CondSignal( canConsume );
	}

	printf( "Producer done.\n" );
}

static void testConsumer( void* data )
{
	printf( "Consumer started\n" );

	for( int i = 0; i < 100; ++i ) {
		SDL_Delay( rand( ) % 1000 );

		SDL_LockMutex( bufferLock ); {
			if( buffer == -1 ) {
				printf( "Consumer encountered empty buffer, waiting for producer to fill it.\n" );
				SDL_CondWait( canConsume, bufferLock );
			}

			printf( "Consumed %i\n", buffer );
			buffer = -1;
		} SDL_UnlockMutex( bufferLock );
		SDL_CondSignal( canProduce );
	}

	printf( "Consumer done.\n" );
}

static void testProduceConsumer( int btnID )
{
	if( prodConsActive ) return;

	prodConsActive = true;
	jq_AddJob( testProducer, NULL );
	jq_AddJob( testConsumer, NULL );
}

static void testJobQueueScreen_Enter( void )
{
	//testImg = img_Load( "Images/tile.png", ST_DEFAULT );
	testImg = -1;
	testFont = -1;
	testSound = -1;

	bufferLock = SDL_CreateMutex( );
	canProduce = SDL_CreateCond( );
	canConsume = SDL_CreateCond( );
	prodConsActive = false;



	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	jq_Initialize( 8 );

	btn_Init( );

	font = txt_LoadFont( "Fonts/Aileron-Regular.otf", 12 );

	//testFont = txt_LoadFont( "Fonts/kenpixel.ttf", 24.0f );

	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );

	btn_Create( vec2( 50.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create Job", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, createJob, NULL );

	btn_Create( vec2( 150.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create 10\nJobs", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, createTenJobs, NULL );

	btn_Create( vec2( 250.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create 100\nJobs", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, createHundredJobs, NULL );

	btn_Create( vec2( 350.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Img\nLoad", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, createDelayedLoadTest, NULL );

	btn_Create( vec2( 450.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Font\nLoad", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, createDelayedFontLoadTest, NULL );

	btn_Create( vec2( 550.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Snd\nLoad",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0 , createDelayedSoundLoadTest, NULL );

	btn_Create( vec2( 50.0f, 150.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Snd\nPlay",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0 , testSoundPlay, NULL );

	btn_Create( vec2( 150.0f, 150.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test\nProd/Cons",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, NULL, whiteImg, CLR_BLUE, 1, 0, testProduceConsumer, NULL );
}

static void testJobQueueScreen_Exit( void )
{
	btn_CleanUp( );

	txt_UnloadFont( font );
	img_Clean( whiteImg );
}

static void testJobQueueScreen_ProcessEvents( SDL_Event* e )
{
}

static void testJobQueueScreen_Process( void )
{
}

static void testJobQueueScreen_Draw( void )
{
	if( img_IsValidImage( testImg ) ) {
		img_CreateDraw( testImg, 1, vec2( 400.0f, 400.0f ), vec2( 400.0f, 400.0f ), 0 );
	}

//#error the string is being displayed all garbled
	//const char* testString = "Testing some string stuff.\nSeeing if it works.";
	const char* testString = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\nTest Test Test   ";
	txt_DisplayString( testString, vec2( 200.0f, 450.0f ), CLR_CYAN,
		HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, testFont, 1, 0, 12.0f );

	txt_DisplayString( testString, vec2( 200.0f, 525.0f ), CLR_CYAN,
		HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, font, 1, 0, 12.0f );
}

static void testJobQueueScreen_PhysicsTick( float dt )
{
}

GameState testJobQueueScreenState = { testJobQueueScreen_Enter, testJobQueueScreen_Exit, testJobQueueScreen_ProcessEvents,
	testJobQueueScreen_Process, testJobQueueScreen_Draw, testJobQueueScreen_PhysicsTick };
