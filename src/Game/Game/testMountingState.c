#include "testMountingState.h"

#include "Graphics/debugRendering.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Math/mathUtil.h"
#include "Input/input.h"
#include "Graphics/camera.h"
#include "Utils/helpers.h"
#include "System/gameTime.h"
#include "Graphics/sprites.h"
#include "Graphics/imageSheets.h"

// just a simple test of mounting and dismounting things. pressing the mouse button will cause a circle to expand from the player
//  and mount any entities hit onto them. wasd to move, 'q' and 'e' keys to rotate, 'o' to start mounting, 'p' to release all attached

static int playerImg = -1;
static int objectImg = -1;
static int* buttonImgs = NULL;

#define NUM_MOUNTABLES 100

static EntityID playerEntity = INVALID_ENTITY_ID;
static EntityID mountables[NUM_MOUNTABLES];

static bool leftPressed = false;
static bool rightPressed = false;
static bool upPressed = false;
static bool downPressed = false;
static bool deosilPressed = false;
static bool widdershinsPressed = false;
static bool bindingPressed = false;
static bool scaleUpPressed = false;
static bool scaleDownPressed = false;

static float bindRadius = 0.0f;

#define LINEAR_SPEED 100.0f
#define ROT_SPEED DEG_TO_RAD( 180.0f )
#define BIND_SIZE_SPEED 100.0f
#define SCALE_SPEED 1.0f
#define MIN_SCALE 0.1f
#define MAX_SCALE 5.0f

static void pressUp( void ) { upPressed = true; }
static void releaseUp( void ) { upPressed = false; }
static void pressDown( void ) { downPressed = true; }
static void releaseDown( void) { downPressed = false; }
static void pressLeft( void ) { leftPressed = true; }
static void releaseLeft( void) { leftPressed = false; }
static void pressRight( void) { rightPressed = true; }
static void releaseRight( void) { rightPressed = false; }
static void pressWiddershins( void ) { widdershinsPressed = true; }
static void releaseWiddershins( void) { widdershinsPressed = false; }
static void pressDeosil( void ) { deosilPressed = true; }
static void releaseDeosil( void ) { deosilPressed = false; }
static void pressBinding( void ) { bindingPressed = true; }
static void pressScaleUp( void ) { scaleUpPressed = true; }
static void releaseScaleUp( void ) { scaleUpPressed = false; }
static void pressScaleDown( void ) { scaleDownPressed = true; }
static void releaseScaleDown( void ) { scaleDownPressed = false; }

static void releaseBinding( void )
{
	bindingPressed = false;

	// find everything within the binding range and mount it
	GCTransformData* playerTransform = NULL;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, playerEntity, gcTransformCompID, &playerTransform ) ) {
		Vector2 basePos = playerTransform->futureState.pos;

		for( size_t i = 0; i < ARRAY_SIZE( mountables ); ++i ) {
			if( mountables[i] == INVALID_ENTITY_ID ) continue;

			GCTransformData* testTf = NULL;
			if( ecps_GetComponentFromEntityByID( &defaultECPS, mountables[i], gcTransformCompID, &testTf ) ) {

				if( testTf->parentID != INVALID_ENTITY_ID ) continue;
				float distSqrd = vec2_DistSqrd( &basePos, &testTf->futureState.pos );
				if( distSqrd > ( bindRadius * bindRadius ) ) continue;

				gc_MountEntity( &defaultECPS, playerEntity, mountables[i] );
			}
		}
	}

	bindRadius = 0.0f;
}

static void pressDrop( void )
{
	// dismount everything mounted to the player entity
	GCTransformData* playerTransform = NULL;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, playerEntity, gcTransformCompID, &playerTransform ) ) {
		while( playerTransform->firstChildID != INVALID_ENTITY_ID ) {
			gc_UnmountEntityFromParent( &defaultECPS, playerTransform->firstChildID );
		}
	}
}

static void pressReset( void )
{
	// reset the player's position to the center of the screen
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	Vector2 center = vec2( renderWidth / 2.0f, renderHeight / 2.0f );
	GCTransformData* playerTransform = NULL;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, playerEntity, gcTransformCompID, &playerTransform ) ) {
		playerTransform->futureState.pos = center;
		playerTransform->currState.pos = center;
	}
}

static EntityID createPlayerEntity( Vector2 pos )
{
	GCTransformData tfData = gc_CreateTransformPos( pos );
	GCSpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 1;
	sprite.img = playerImg;

	return ecps_CreateEntity( &defaultECPS, 2,
		gcTransformCompID, &tfData,
		gcSpriteCompID, &sprite );
}

static EntityID createMountableEntity( Vector2 pos )
{
	GCTransformData tfData = gc_CreateTransformPos( pos );
	//GCTransformData tfData = gc_CreateTransformPosRot( pos, M_PI_F );
	//GCTransformData tfData = gc_CreateTransformPosScale( pos, vec2( -1.0f, -1.0f ) );
	GCSpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 0;
	sprite.img = objectImg;

	return ecps_CreateEntity( &defaultECPS, 2,
		gcTransformCompID, &tfData,
		gcSpriteCompID, &sprite );
}

static int debugPos = 0;
static Vector2 debugTopLefts[NUM_MOUNTABLES];
static Vector2 debugSize;

static EntityID createMountable3x3Entity( Vector2 pos )
{
	float baseSize = 16.0f;
	float scale = 2.0f;
	debugTopLefts[debugPos] = pos;
	debugTopLefts[debugPos].x -= ( baseSize / 2.0f ) * scale;
	debugTopLefts[debugPos].y -= ( baseSize / 2.0f ) * scale;
	++debugPos;
	debugSize = vec2( baseSize * scale, baseSize * scale );

	//GCTransformData tfData = gc_CreateTransformPos( pos );
	GCTransformData tfData = gc_CreateTransformPosScale( pos, vec2( scale, scale ) );
	GC3x3SpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 0;
	//sprite.size = vec2( 32.0f, 16.0f );
	sprite.size = vec2( baseSize, baseSize );
	memcpy( sprite.imgs, buttonImgs, sizeof( sprite.imgs ) );

	return ecps_CreateEntity( &defaultECPS, 2,
		gcTransformCompID, &tfData,
		gc3x3SpriteCompID, &sprite );
}

static int testMountingState_Enter( void )
{
	/*ecps_StartInitialization( &defaultECPS ); {
		gameComponents_Setup( &defaultECPS );
		gameProcesses_Setup( &defaultECPS );
	} ecps_FinishInitialization( &defaultECPS );//*/

	cam_SetFlags( 0, 1 );
	gfx_SetClearColor( clr( 0.1f, 0.0f, 0.1f, 1.0f ) );
	
	playerImg = img_Load( "Images/marker.png", ST_DEFAULT );
	objectImg = img_Load( "Images/color_piece.png", ST_DEFAULT );

	img_LoadSpriteSheet( "Images/button.spritesheet", ST_DEFAULT, &buttonImgs );

	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );

	Vector2 center = vec2( renderWidth / 2.0f, renderHeight / 2.0f );

	playerEntity = createPlayerEntity( center );

	// create mountable entities
	int idx = 0;
	int numX = (int)SDL_sqrt( NUM_MOUNTABLES );
	int numY = NUM_MOUNTABLES / numX;
	float stepX = (float)renderWidth / ( numX + 1 );
	float stepY = (float)renderHeight / ( numY + 1 );
	for( int x = 0; x < numX; ++x ) {
		for( int y = 0; y < numY; ++y ) {
			mountables[idx] = createMountableEntity( vec2( stepX + ( stepX * x ), stepY + ( stepY * y ) ) );
			//mountables[idx] = createMountable3x3Entity( vec2( stepX + ( stepX * x ), stepY + ( stepY * y ) ) );
			++idx;
		}
	}

	for( ; idx < NUM_MOUNTABLES; ++idx ) {
		mountables[idx] = INVALID_ENTITY_ID;
	}

	// set up input
	input_BindOnKey( SDLK_W, pressUp, releaseUp );
	input_BindOnKey( SDLK_S, pressDown, releaseDown );
	input_BindOnKey( SDLK_A, pressLeft, releaseLeft );
	input_BindOnKey( SDLK_D, pressRight, releaseRight );
	input_BindOnKey( SDLK_Q, pressWiddershins, releaseWiddershins );
	input_BindOnKey( SDLK_E, pressDeosil, releaseDeosil );
	input_BindOnKey( SDLK_O, pressBinding, releaseBinding );
	input_BindOnKey( SDLK_P, pressDrop, NULL );
	input_BindOnKey( SDLK_R, pressReset, NULL );
	input_BindOnKey( SDLK_EQUALS, pressScaleUp, releaseScaleUp );
	input_BindOnKey( SDLK_MINUS, pressScaleDown, releaseScaleDown );

	return 1;
}

static int testMountingState_Exit( void )
{
	return 1;
}

static void testMountingState_ProcessEvents( SDL_Event* e )
{
}

static void testMountingState_Process( void )
{
}

static void testMountingState_Draw( void )
{
	GCTransformData* playerTransform = NULL;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, playerEntity, gcTransformCompID, &playerTransform ) ) {
		if( bindRadius >= 1.0f ) {
			debugRenderer_Circle( 1, playerTransform->futureState.pos, bindRadius, CLR_BLUE );
		}
		//debugRenderer_Circle( 1, playerTransform->futureState.pos, 5.0f, CLR_GREEN );
	}

	//img_CreateDraw( objectImg, 1, vec2( 400.0f, 300.0f ), vec2( 400.0f, 300.0f ), 10 );

	for( int i = 0; i < NUM_MOUNTABLES; ++i ) {
		debugRenderer_AABB( 1, debugTopLefts[i], debugSize, CLR_GREEN );
	}
}

static void testMountingState_PhysicsTick( float dt )
{
	//customClear( );

	Vector2 linearOffset = VEC2_ZERO;
	float rotOffset = 0.0f;
	float scaleOffset = 0.0f;

	if( leftPressed ) linearOffset.x -= LINEAR_SPEED;
	if( rightPressed ) linearOffset.x += LINEAR_SPEED;
	if( upPressed ) linearOffset.y -= LINEAR_SPEED;
	if( downPressed ) linearOffset.y += LINEAR_SPEED;
	if( deosilPressed ) rotOffset += ROT_SPEED;
	if( widdershinsPressed ) rotOffset -= ROT_SPEED;
	if( scaleUpPressed ) scaleOffset += SCALE_SPEED;
	if( scaleDownPressed ) scaleOffset -= SCALE_SPEED;

	linearOffset.x *= dt;
	linearOffset.y *= dt;
	rotOffset *= dt;
	scaleOffset = scaleOffset * dt;

	GCTransformData* playerTransform = NULL;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, playerEntity, gcTransformCompID, &playerTransform ) ) {

		gc_AdjustLocalPos( playerTransform, &linearOffset );
		gc_AdjustLocalRot( playerTransform, rotOffset );
		Vector2 scale = gc_GetLocalScale( playerTransform );
		scale.x = clamp( MIN_SCALE, MAX_SCALE, ( scale.x + scaleOffset ) );
		scale.y = scale.x;
		gc_SetTransformLocalVectorScale( playerTransform, scale );
	}

	if( bindingPressed ) bindRadius += BIND_SIZE_SPEED * dt;
}

static void testMountingState_Render( float t )
{
}

GameState testMountingState = { testMountingState_Enter, testMountingState_Exit, testMountingState_ProcessEvents,
	testMountingState_Process, testMountingState_Draw, testMountingState_PhysicsTick, testMountingState_Render };
