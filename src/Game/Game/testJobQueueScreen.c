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

#include "UI/uiEntities.h"

#include "System/jobQueue.h"

#include "Audio/sound.h"

#include "DefaultECPS/defaultECPS.h"

static int font;
static int whiteImg;

static ImageID testImg;
static int testFont;

static int testSound;

// baseic producer/consumer setup to allow
static bool prodConsActive = false;
static SDL_Mutex* bufferLock = NULL;
static SDL_Condition* canProduce = NULL;
static SDL_Condition* canConsume = NULL;
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

static void createJob( ECPS* ecps, Entity* btn )
{
	jq_AddJob( testJob, NULL );
}

static void createTenJobs( ECPS* ecps, Entity* btn )
{
	//jq_AddJob( addTenJobs, NULL );
	addTenJobs( NULL );
}

static void createHundredJobs( ECPS* ecps, Entity* btn )
{
	//jq_AddJob( addHundredJobs, NULL );
	addHundredJobs( NULL );
}

static void createDelayedLoadTest( ECPS* ecps, Entity* btn )
{
	jq_AddJob( delayedLoadTest, NULL );
}

static void createDelayedFontLoadTest( ECPS* ecps, Entity* btn )
{
	jq_AddJob( delayedFontLoadTest, NULL );
}

static void createDelayedSoundLoadTest( ECPS* ecps, Entity* btn )
{
	jq_AddJob( delayedSoundLoadTest, NULL );
}

static void testSoundPlay( ECPS* ecps, Entity* btn )
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
				SDL_WaitCondition( canProduce, bufferLock );
			}

			buffer = rand( ) % 255;
			printf( "Produced %i\n", buffer );
		} SDL_UnlockMutex( bufferLock );
		SDL_SignalCondition( canConsume );
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
				SDL_WaitCondition( canConsume, bufferLock );
			}

			printf( "Consumed %i\n", buffer );
			buffer = -1;
		} SDL_UnlockMutex( bufferLock );
		SDL_SignalCondition( canProduce );
	}

	printf( "Consumer done.\n" );
}

static void testProduceConsumer( ECPS* ecps, Entity* btn )
{
	if( prodConsActive ) return;

	prodConsActive = true;
	jq_AddJob( testProducer, NULL );
	jq_AddJob( testConsumer, NULL );
}

static void testJobQueueScreen_Enter( void )
{
	//testImg = img_Load( "Images/tile.png", ST_DEFAULT );
	testImg = INVALID_IMAGE_ID;
	testFont = -1;
	testSound = -1;

	bufferLock = SDL_CreateMutex( );
	canProduce = SDL_CreateCondition( );
	canConsume = SDL_CreateCondition( );
	prodConsActive = false;

	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	jq_Initialize( 8 );

	font = txt_LoadFont( "Fonts/Aileron-Regular.otf", 12 );

	//testFont = txt_LoadFont( "Fonts/kenpixel.ttf", 24.0f );

	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );

	button_CreateImageButton( &defaultECPS, vec2( 50.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create Job", 
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, createJob, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 150.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create 10\nJobs",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, createTenJobs, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 250.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Create 100\nJobs",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, createHundredJobs, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 350.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Img\nLoad",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, createDelayedLoadTest, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 450.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Font\nLoad",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, createDelayedFontLoadTest, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 550.0f, 50.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Snd\nLoad",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0 , createDelayedSoundLoadTest, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 50.0f, 150.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test Snd\nPlay",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0 , testSoundPlay, NULL, NULL, NULL );

	button_CreateImageButton( &defaultECPS, vec2( 150.0f, 150.0f ), vec2( 50.0f, 50.0f ), vec2( 60.0f, 60.0f ), "Test\nProd/Cons",
		font, 12.0f, CLR_WHITE, VEC2_ZERO, whiteImg, CLR_BLUE, 1, 0, testProduceConsumer, NULL, NULL, NULL );
}

static void testJobQueueScreen_Exit( void )
{
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

}

static void testJobQueueScreen_PhysicsTick( float dt )
{
}

static void testJobQueueScreen_Render( float t )
{
	if( img_IsValidImage( testImg ) ) {
		Vector2 pos = vec2( 400.0f, 400.0f );
		img_Render_Pos( testImg, 1, 0, &pos );
	}

	const char* testString = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\nTest Test Test   ";
	txt_DisplayString( (const uint8_t*)testString, vec2( 200.0f, 450.0f ), CLR_CYAN,
		HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, testFont, 1, 0, 12.0f );

	txt_DisplayString( (const uint8_t*)testString, vec2( 200.0f, 525.0f ), CLR_CYAN,
		HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, font, 1, 0, 12.0f );
}

GameState testJobQueueScreenState = { testJobQueueScreen_Enter, testJobQueueScreen_Exit, testJobQueueScreen_ProcessEvents,
	testJobQueueScreen_Process, testJobQueueScreen_Draw, testJobQueueScreen_PhysicsTick, testJobQueueScreen_Render };
