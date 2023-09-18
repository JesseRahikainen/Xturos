#include "testPointerResponseScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../System/ECPS/entityComponentProcessSystem.h"

#include "../Components/generalComponents.h"
#include "../Processes/generalProcesses.h"

#include "../System/platformLog.h"

#include "../System/ECPS/ecps_trackedCallbacks.h"

static ECPS ecps;
static int whiteImg;
static Vector2 imgSize;

static void onButtonOver( ECPS* btnEcps, Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_GREY;
}
ADD_TRACKED_ECPS_CALLBACK( "TEST_ON_BUTTON_OVER", onButtonOver );

static void onButtonLeave( ECPS* btnEcps, Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_WHITE;
}
ADD_TRACKED_ECPS_CALLBACK( "TEST_ON_BUTTON_LEAVE", onButtonLeave );

static void onButtonPress( ECPS* btnEcps, Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_RED;
}
ADD_TRACKED_ECPS_CALLBACK( "TEST_ON_BUTTON_PRESS", onButtonPress );

static void onButtonRelease( ECPS* btnEcps, Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_YELLOW;
}
ADD_TRACKED_ECPS_CALLBACK( "TEST_ON_BUTTON_RELEASE", onButtonRelease );

static void createTestButton( Vector2 position, Vector2 size )
{
	GCPosData pos;
	pos.currPos = pos.futurePos = position;

	GCColorData clr;
	clr.currClr = clr.futureClr = CLR_WHITE;

	GCPointerResponseData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &size, 0.5f, &(ptr.collisionHalfDim ) );
	ptr.leaveResponse = onButtonLeave;
	ptr.overResponse = onButtonOver;
	ptr.pressResponse = onButtonPress;
	ptr.releaseResponse = onButtonRelease;
	ptr.priority = 1;

	GCSpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 0;
	sprite.img = whiteImg;

	GCScaleData scale;
	scale.currScale.x = size.x / imgSize.x;
	scale.currScale.y = size.y / imgSize.y;
	scale.futureScale = scale.currScale;

	GCGroupIDData group;
	group.groupID = 1;

	ecps_CreateEntity( &ecps, 6,
		gcPosCompID, &pos,
		gcClrCompID, &clr,
		gcPointerResponseCompID, &ptr,
		gcSpriteCompID, &sprite,
		gcScaleCompID, &scale,
		gcGroupIDCompID, &group );
}

#include "System/ECPS/ecps_serialization.h"
#include "Utils/stretchyBuffer.h"

static size_t buttonCount = 0;
void countButtonsStart( ECPS* gameECPS )
{
	buttonCount = 0;
}

void countButtons( ECPS* gameECPS, const Entity* entity )
{
	++buttonCount;
}

static void testPointerResponseScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );
	img_GetSize( whiteImg, &imgSize );

	ecps_StartInitialization( &ecps ); {
		gc_Register( &ecps );
		gp_RegisterProcesses( &ecps );
	} ecps_FinishInitialization( &ecps );

	createTestButton( vec2( 400.0f, 300.0f ), vec2( 100.0f, 100.0f ) );
	createTestButton( vec2( 375.0f, 275.0f ), vec2( 100.0f, 100.0f ) );
	createTestButton( vec2( 350.0f, 250.0f ), vec2( 100.0f, 100.0f ) );
	createTestButton( vec2( 325.0f, 225.0f ), vec2( 100.0f, 100.0f ) );

	// serialize the buttons
	SerializedECPS serializedECPS;
	ecps_InitSerialized( &serializedECPS );

	ecps_GenerateSerializedECPS( &ecps, &serializedECPS );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &ecps, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	assert( buttonCount == 4 );

	// destroy all buttons
	gp_DeleteAllOfGroup( &ecps, 1 );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &ecps, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	assert( buttonCount == 0 );

	// try saving out and loading in
	ecps_SaveSerializedECPS( "test.pkg", &serializedECPS );
	ecps_CleanSerialized( &serializedECPS );
	ecps_LoadSerializedECPS( "test.pkg", &serializedECPS );

	// deserialize the buttons
	ecps_CreateEntitiesFromSerializedComponents( &ecps, &serializedECPS );
	
	// clean up
	ecps_CleanSerialized( &serializedECPS );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &ecps, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	assert( buttonCount == 4 );//*/
}

static void testPointerResponseScreen_Exit( void )
{
}

static void testPointerResponseScreen_ProcessEvents( SDL_Event* e )
{
	gp_PointerResponseEventHandler( e );
}

static void testPointerResponseScreen_Process( void )
{
	ecps_RunProcess( &ecps, &gpPointerResponseProc );
}

static void testPointerResponseScreen_Draw( void )
{
	ecps_RunProcess( &ecps, &gpRenderProc );
}

static void testPointerResponseScreen_PhysicsTick( float dt )
{
}

GameState testPointerResponseScreenState = { testPointerResponseScreen_Enter, testPointerResponseScreen_Exit, testPointerResponseScreen_ProcessEvents,
	testPointerResponseScreen_Process, testPointerResponseScreen_Draw, testPointerResponseScreen_PhysicsTick };