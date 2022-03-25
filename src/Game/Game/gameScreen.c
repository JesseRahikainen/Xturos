#include "gameScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../System/platformLog.h"
#include "../System/random.h"
#include "../Utils/helpers.h"

#include "../System/ECPS/entityComponentProcessSystem.h"

static int templateID[2];
static int instanceID[2];
static Vector2 pos[5] = {	{ 300.0f, 200.0f },
							{ 300.0f, 550.0f },
							{ 300.0f, 300.0f },
							{ 200.0f, 300.0f },
							{ 500.0f, 300.0f } };

static int testImages[3];
static char* fileNames[3] = { "Images/test.png", "Images/test_transparent.png", "Images/test2.png" };

static int* imageSheet;
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


//#error adding the component to the entity breaks things
	// but we'd really like to be able to add components to things at run time, an example of this is an entity that self-destructs
	//  after a certain amount of time, we'd need to test to see if the appropriate amount of time has passed and if it has then
	//  destroy it
	// which also adds the issue of does destroying an entity cause problems, no, since we're not modifying the array itself but
	//  just removing the entry for that entity and then zeroing out it's memory
	// maybe if we try to add an component while we're processing we just add that to an instruction queue that is run after the
	//  process is done running, that won't work for processes that needs to add data that must be set immediately
	// what if we did a bigger structural change. the ecps would have two buffers, one for read, one for write. there would then
	//  be a swapBuffers command that would copy the contents of the write into the read. but we're relying on having the pointer
	//  for a component type to be used for reading and writing. maybe we just have two types of those pointers, a readPointer and
	//  a writePointer.
	//
	//  //ecps_GetEntityReadComponent( ecps, entity, timerCompID, &readTimerData );
	//  //ecps_GetEntityWriteComponent( ecps, entity, timerCompID, &writeTimerData );
	//  ecps_GetEntityComponent( ecps, entity, timerCompID, &readTimerData, &writeTimerData );
	//  if( readTimerData->timePassed >= readTimerData->timeAllowed ) {
	//      ecps_DestroyEntity( ecps, entity );
	//      return;
	//  }
	//  writeTimerData->timePassed = readTimerData->timePassed + dt;
	//
	//  that would allow us to do stuff like:
	//  ecps_GetEntityComponent( ecps, entity, timerCompID, &readTimerData, &writeTimerData );
	//  writeTimerData->timePassed = readTimerData->timePassed + dt;
	//  if( readTimerData->timePassed >= readTimerData->timeAllowed ) {
	//      ecps_DestroyEntity( ecps, entity );
	//      return;
	//  } else if( readTimerData->timePassed >= readTimerData->timeAllowed * 0.5f ) {
	//      ecps_AddComponent( ecps, entity, 
	//  }
	//
	// there has to be a better way to do this...
	//
	// unity seems to be using a command pattern, so instead of directly modifying the data, they
	//  create a command that will update the data at a certain point
	// given that we'll be updating components almost every single frame, the command pattern may
	//  not give space savings versus the double buffer pattern
	//
	// first things first, lets rewrite the addComponent to take in a void* that should be the data
	//  used for the component. e.g.:
	//   PosData pos;
	//   pos.curr = vec2( 0.0f, 0.0f );
	//   pos.next = pos.curr;
	//   ecps_AddComponentToEntity( entity, posCompID, &pos );
	// this will make any future changes easier as all the data will be there instead of having
	//  to worry about memory accesses
	//
	// for creation lets do something similar, instead of retrieving the components and writing to
	//  them pass in an interleaved list of { ComponentID id, void* data }
	// this will make the addition and creation similar enough to avoid dissonance
	// we could also do this in a way such that we can create templates that we can feed into the
	//  creation function, but lets not worry about that for right now
	//
	// so, we've done all that
	// now this still doesn't solve our original issue: how to cleanly add components to entities
	//  especially during a run of a process
	// artemis uses a command buffer, unity3d seems to use a command buffer as well
	// if we assume that we run the command buffer after running each process then it should never
	//  get too big. we can also assum that for right now we edit in place, so the only commands
	//  we'll need are AddComponentToEntity, RemoveComponentFromEntity, CreateEntity and DeleteEntity
	//  this also means we won't need the CleanUp process that we've been using
	// we'll use one base function for each of these, internally they'll check to see if a
	//  process is being run, if one is it'll add commands to the queue, otherwise it'll process
	//  it immediately
	
//#error what do we need to test?
	/*
	Create entity
	  - while processing
	  - not while processing
	Destroy entity
	  - while processing
	  - not while processing
	Add component to entity
	  - while processing
	  - not while processing
	Remove component from entity
	  - while processing
	  - not while processing
	*/

static int gameScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	spriteSheetImg = img_Load( "Images/spritesheet_test.png", ST_DEFAULT );

	numImagesOnSheet = img_LoadSpriteSheet( "Images/spritesheet_test.ss", ST_DEFAULT, &imageSheet );

	/*
	templateID[0] = spine_LoadTemplate( "Spines/JSON/yellow_mob" );
	instanceID[0] = spine_CreateInstance( templateID[0], pos[0], 1, 0, NULL );
	spAnimationState_setAnimationByName( spine_GetInstanceAnimState( instanceID[0] ), 0, "idle", 1 ); //*/

	/*
	templateID[1] = spine_LoadTemplate( "Spines/Goblin/goblins-ffd" );
	instanceID[1] = spine_CreateInstance( templateID[1], pos[1], 1, 0, NULL );
	spAnimationState_setAnimationByName( spine_GetInstanceAnimState( instanceID[1] ), 0, "walk", 1 );
	spSkeleton_setSkinByName( spine_GetInstanceSkeleton( instanceID[1] ), "goblin" ); /*
	spSkeleton_setSkinByName( spine_GetInstanceSkeleton( instanceID[1] ), "goblingirl" ); // */

	//img_LoadPackage( 3, fileNames, testImages );

	font = txt_LoadFont( "Fonts/kenpixel.ttf", 128 );

	ecps_StartInitialization( &testECPS ); {
		spawningCompID = ecps_AddComponentType( &testECPS, "SPAWN", 0, 0, NULL, NULL );
		attackingCompID = ecps_AddComponentType( &testECPS, "ATTACK", sizeof( AttackingData ), ALIGN_OF( AttackingData ), NULL, NULL );
		selfDestructCompID = ecps_AddComponentType( &testECPS, "Sudoku", sizeof( SelfDestructData ), ALIGN_OF( SelfDestructData ), NULL, NULL );

		ecps_CreateProcess( &testECPS, "SPAWN", NULL, spawning, NULL, &spawnProc, 1, spawningCompID );
		ecps_CreateProcess( &testECPS, "ATTACK", NULL, attacking, NULL, &attackProc, 1, attackingCompID );
		ecps_CreateProcess( &testECPS, "SUDOKU", NULL, selfDestructing, NULL, &selfDestructProc, 1, selfDestructCompID );
	} ecps_FinishInitialization( &testECPS );

	return 1;
}

static int gameScreen_Exit( void )
{
	return 1;
}

static void gameScreen_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Process( void )
{
	
}

static void gameScreen_Draw( void )
{
	//img_CreateDraw( spriteSheetImg, 1, pos[0], pos[0], 0 );
	for( int i = 0; i < numImagesOnSheet; ++i ) {
		img_CreateDraw( imageSheet[i], 1, pos[i], pos[i], 0 );
	}

	txt_DisplayString( "Testing stuff\nAnd even more stuff\nAnd once more", VEC2_ZERO, CLR_RED, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, font, 1, 0, 128.0f );//*/

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

GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };