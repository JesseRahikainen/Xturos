#include "testBloomScreen.h"

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/spineGfx.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "Graphics/sprites.h"
#include "Utils/helpers.h"
#include "world.h"
#include "System/random.h"

static int whiteImg;
static int markerImg;

static float left;
static float right;
static float top;
static float bottom;

static Vector2 boxSize;

typedef struct {
	EntityID img;
	Vector2 velocity;
	Vector2 pos;
} MovingThing;
static MovingThing movingThings[40];

static int testBloomScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	gfx_SetClearColor( CLR_BLACK );

	spr_Init( );

	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );
	markerImg = img_Load( "Images/marker.png", ST_DEFAULT );

	Vector2 size;
	boxSize = VEC2_ZERO;
	img_GetSize( whiteImg, &boxSize );
	img_GetSize( markerImg, &size );

	boxSize.x = MAX( boxSize.x, size.x );
	boxSize.y = MAX( boxSize.y, size.y );

	Vector2 worldSize;
	world_GetSize( &worldSize );

	left = boxSize.x / 2.0f;
	right = worldSize.x - left;
	top = boxSize.y / 2.0f;
	bottom = worldSize.y - top;



	for( size_t i = 0; i < ARRAY_SIZE( movingThings ); ++i ) {
		int img = whiteImg;
		if( ( i % 2 ) == 0 ) {
			img = markerImg;
		}

		float speed = rand_GetRangeFloat( NULL, 15.0f, 60.0f );
		Vector2 pos = vec2( rand_GetRangeFloat( NULL, left, right ), rand_GetRangeFloat( NULL, top, bottom ) );
		movingThings[i].pos = pos;
		movingThings[i].img = spr_CreateSprite( img, 1, pos, VEC2_ONE, 0.0f, CLR_WHITE, 0 );
		movingThings[i].velocity.x = ( rand_Choice( NULL ) ? 1.0f : -1.0f ) * speed;
		movingThings[i].velocity.y = ( rand_Choice( NULL ) ? 1.0f : -1.0f ) * speed;

		if( i > ( ARRAY_SIZE( movingThings ) / 2 ) ) {
			// make glow!
		}
	}

	return 1;
}

static int testBloomScreen_Exit( void )
{
	spr_CleanUp( );
	return 1;
}

static void testBloomScreen_ProcessEvents( SDL_Event* e )
{}

static void testBloomScreen_Process( void )
{}

static void testBloomScreen_Draw( void )
{
	
}

static void testBloomScreen_PhysicsTick( float dt )
{
	for( size_t i = 0; i < ARRAY_SIZE( movingThings ); ++i ) {
		vec2_AddScaled( &movingThings[i].pos, &movingThings[i].velocity, dt, &movingThings[i].pos );

		if( movingThings[i].pos.x <= left ) {
			movingThings[i].velocity.x = SDL_fabsf( movingThings[i].velocity.x );
		} else if( movingThings[i].pos.x >= right ) {
			movingThings[i].velocity.x = -SDL_fabsf( movingThings[i].velocity.x );
		}

		if( movingThings[i].pos.y <= top ) {
			movingThings[i].velocity.y = SDL_fabsf( movingThings[i].velocity.y );
		} else if( movingThings[i].pos.y >= bottom ) {
			movingThings[i].velocity.y = -SDL_fabsf( movingThings[i].velocity.y );
		}

		spr_Update_p( movingThings[i].img, &movingThings[i].pos );
	}
}

GameState testBloomScreenState = { testBloomScreen_Enter, testBloomScreen_Exit, testBloomScreen_ProcessEvents,
	testBloomScreen_Process, testBloomScreen_Draw, testBloomScreen_PhysicsTick };