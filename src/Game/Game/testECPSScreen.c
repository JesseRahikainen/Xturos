#include "testECPSScreen.h"

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "UI/text.h"
#include "System/platformLog.h"
#include "System/random.h"
#include "Utils/helpers.h"

#include "System/ECPS/entityComponentProcessSystem.h"

static int templateID[2];
static int instanceID[2];
static Vector2 pos[5] = {	{ 300.0f, 200.0f },
							{ 300.0f, 550.0f },
							{ 300.0f, 300.0f },
							{ 200.0f, 300.0f },
							{ 500.0f, 300.0f } };

static int testImages[3];
static char* fileNames[3] = { "Images/test.png", "Images/test_transparent.png", "Images/test2.png" };

static int* sbImageSheet;
static int numImagesOnSheet;

static int spriteSheetImg;

static int font;

static ECPS testECPS;

typedef struct {
	int numTicksPassed;
	EntityID targetEntity;
} AttackingData;
static ComponentID attackingCompID = INVALID_COMPONENT_ID;

typedef struct {
	int numTicksPassed;
} SelfDestructData;
static ComponentID selfDestructCompID = INVALID_COMPONENT_ID;

static ComponentID spawningCompID = INVALID_COMPONENT_ID;

static Process spawnProc;
static void spawning( ECPS* ecps, const Entity* entity )
{
	llog( LOG_DEBUG, " Spawning entities" );
	EntityID entities[2] = { INVALID_ENTITY_ID, INVALID_ENTITY_ID };

	for( int i = 0; i < ARRAY_SIZE( entities ); ++i ) {
		if( rand_Choice( NULL ) ) {
			llog( LOG_DEBUG, " - Is an attacker" );
			AttackingData data;
			data.numTicksPassed = 0;
			data.targetEntity = entities[1-i];

			entities[i] = ecps_CreateEntity( ecps, 1, attackingCompID, &data );
		} else {
			llog( LOG_DEBUG, " - Is a self-destructer" );
			SelfDestructData data;
			data.numTicksPassed = 0;

			entities[i] = ecps_CreateEntity( ecps, 1, selfDestructCompID, &data );
		}
	}
}

#define ATTACK_PERIOD 5
static Process attackProc;
static void attacking( ECPS* ecps, const Entity* entity )
{
	AttackingData* attacking = NULL;
	ecps_GetComponentFromEntity( entity, attackingCompID, &attacking );

	++( attacking->numTicksPassed );
	if( ecps_GetEntityByID( ecps, attacking->targetEntity, NULL ) ) {
		llog( LOG_DEBUG, " Attacking enemy, %i of %i", attacking->numTicksPassed, ATTACK_PERIOD );
		if( rand_GetRangeS32( NULL, attacking->numTicksPassed, ATTACK_PERIOD ) == ATTACK_PERIOD ) {
			llog( LOG_DEBUG, "  - Killing opponent" );
			ecps_DestroyEntityByID( ecps, attacking->targetEntity );
			attacking->targetEntity = INVALID_ENTITY_ID;
		} else {
			llog( LOG_DEBUG, "  - Unable to kill opponent" );
		}
	} else {
		llog( LOG_DEBUG, " No one to kill, committing sudoku" );
		ecps_RemoveComponentFromEntity( ecps, (Entity*)entity, attackingCompID );
		SelfDestructData data;
		data.numTicksPassed = 0;
		ecps_AddComponentToEntity( ecps, (Entity*)entity, selfDestructCompID, &data );
	}
}

#define SELF_DESTRUCT_PERIOD 10
static Process selfDestructProc;
static void selfDestructing( ECPS* ecps, const Entity* entity )
{
	SelfDestructData* selfDestruct = NULL;
	ecps_GetComponentFromEntity( entity, selfDestructCompID, &selfDestruct );

	llog( LOG_DEBUG, " Getting ready to self destruct" );
	++( selfDestruct->numTicksPassed );
	if( rand_GetRangeS32( NULL, selfDestruct->numTicksPassed, SELF_DESTRUCT_PERIOD ) == SELF_DESTRUCT_PERIOD ) {
		llog( LOG_DEBUG, "  - Dead..." );
		ecps_DestroyEntity( ecps, entity );
	}
}

static void gameScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	spriteSheetImg = img_Load( "Images/test.spritesheet_0.png", ST_DEFAULT );

	int packageID = img_LoadSpriteSheet( "Images/test.spritesheet", ST_DEFAULT, &sbImageSheet );
	numImagesOnSheet = (int)img_GetPackageImageCount( packageID );

	font = txt_LoadFont( "Fonts/Aileron-Regular.otf", 128 );

	ecps_StartInitialization( &testECPS ); {
		spawningCompID = ecps_AddComponentType( &testECPS, "SPAWN", 0, 0, 0, NULL, NULL, NULL, NULL );
		attackingCompID = ecps_AddComponentType( &testECPS, "ATTACK", 0, sizeof( AttackingData ), ALIGN_OF( AttackingData ), NULL, NULL, NULL, NULL );
		selfDestructCompID = ecps_AddComponentType( &testECPS, "Sudoku", 0, sizeof( SelfDestructData ), ALIGN_OF( SelfDestructData ), NULL, NULL, NULL, NULL );

		ecps_CreateProcess( &testECPS, "SPAWN", NULL, spawning, NULL, &spawnProc, 1, spawningCompID );
		ecps_CreateProcess( &testECPS, "ATTACK", NULL, attacking, NULL, &attackProc, 1, attackingCompID );
		ecps_CreateProcess( &testECPS, "SUDOKU", NULL, selfDestructing, NULL, &selfDestructProc, 1, selfDestructCompID );
	} ecps_FinishInitialization( &testECPS );
}

static void gameScreen_Exit( void )
{
}

static void gameScreen_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Process( void )
{
	
}

static void gameScreen_Draw( void )
{
	debugRenderer_Circle( 1, VEC2_ZERO, 100.0f, CLR_RED );
}

static EntityID spawnerEntity = INVALID_ENTITY_ID;
#define SPAWNER_DEAD_PERIOD 10
#define SPAWNER_WAIT_PERIOD 10
#define SPAWNER_ACTIVE_PERIOD 10
static int currPeriod = 0;

static void gameScreen_PhysicsTick( float dt )
{
	/*llog( LOG_DEBUG, "Starting process" );

	++currPeriod;
	if( spawnerEntity == INVALID_ENTITY_ID ) {
		if( rand_GetRangeS32( NULL, currPeriod, SPAWNER_DEAD_PERIOD ) == SPAWNER_DEAD_PERIOD ) {
			llog( LOG_DEBUG, " Spawner is born" );
			spawnerEntity = ecps_CreateEntity( &testECPS, 1, spawningCompID, NULL );
			currPeriod = 0;
		} else {
			llog( LOG_DEBUG, " Spawner does not exist" );
		}
	} else if( ecps_DoesEntityHaveComponentByID( &testECPS, spawnerEntity, spawningCompID ) ) {
		if( rand_GetRangeS32( NULL, currPeriod, SPAWNER_ACTIVE_PERIOD ) == SPAWNER_ACTIVE_PERIOD ) {
			llog( LOG_DEBUG, " Spawner is now infertile" );
			ecps_RemoveComponentFromEntityByID( &testECPS, spawnerEntity, spawningCompID );
			currPeriod = 0;
		}
	} else {
		if( rand_GetRangeS32( NULL, currPeriod, SPAWNER_WAIT_PERIOD ) == SPAWNER_WAIT_PERIOD ) {
			llog( LOG_DEBUG, " Spawner dead" );
			ecps_DestroyEntityByID( &testECPS, spawnerEntity );
			spawnerEntity = INVALID_ENTITY_ID;
			currPeriod = 0;
		} else {
			llog( LOG_DEBUG, " Spawner is waiting to die" );
		}
	}

	ecps_RunProcess( &testECPS, &spawnProc );
	ecps_RunProcess( &testECPS, &attackProc );
	ecps_RunProcess( &testECPS, &selfDestructProc );//*/
}

static void gameScreen_Render( float t )
{
	for( int i = 0; i < numImagesOnSheet; ++i ) {
		img_Render_Pos( sbImageSheet[i], 1, 0, &pos[i] );
	}

	txt_DisplayString( "Testing stuff\nAnd even more stuff\nAnd once more", VEC2_ZERO, CLR_RED, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, font, 1, 0, 128.0f );
}

GameState testECPSScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick, gameScreen_Render };