#include "testPointerResponseScreen.h"

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"

#include "System/ECPS/entityComponentProcessSystem.h"

#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"

#include "System/platformLog.h"

#include "System/ECPS/ecps_trackedCallbacks.h"

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

static EntityID createTestButton( Vector2 position, Vector2 size )
{
	Vector2 scale = vec2( size.x / imgSize.x, size.y / imgSize.y );
	GCTransformData tf = gc_CreateTransformPosScale( position, scale );

	GCColorData clr;
	clr.currClr = clr.futureClr = CLR_WHITE;

	GCPointerCollisionData ptr;
	ptr.camFlags = 1;
	vec2_Scale( &size, 0.5f, &(ptr.collisionHalfDim ) );
	ptr.priority = 1;

	GCPointerResponseData response;
	response.leaveResponse = createSourceCallback( onButtonLeave );
	response.overResponse = createSourceCallback( onButtonOver );
	response.pressResponse = createSourceCallback( onButtonPress );
	response.releaseResponse = createSourceCallback( onButtonRelease );

	GCSpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 0;
	sprite.img = whiteImg;

	GCGroupIDData group;
	group.groupID = 1;

	return ecps_CreateEntity( &defaultECPS, 6,
		gcTransformCompID, &tf,
		gcClrCompID, &clr,
		gcPointerCollisionCompID, &ptr,
		gcPointerResponseCompID, &response,
		gcSpriteCompID, &sprite,
		gcGroupIDCompID, &group );
}

#include "System/ECPS/ecps_fileSerialization.h"
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

void rotateButtons( ECPS* gameECPS, const Entity* entity )
{
	GCTransformData* tf;
	if( !ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf ) ) return;

	if( tf->parentID != INVALID_ENTITY_ID ) return;

	gc_AdjustLocalRot( tf, 0.01f );
}

static void testPointerResponseScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );
	img_GetSize( whiteImg, &imgSize );

	/*ecps_StartInitialization( &ecps ); {
		gc_Register( &ecps );
		gp_RegisterProcesses( &ecps );
	} ecps_FinishInitialization( &ecps );//*/

	SerializedECPS serializedECPS;
	ecps_InitSerialized( &serializedECPS );

	EntityID btn0 = createTestButton( vec2( 400.0f, 300.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn1 = createTestButton( vec2( 375.0f, 275.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn2 = createTestButton( vec2( 350.0f, 250.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn3 = createTestButton( vec2( 325.0f, 225.0f ), vec2( 100.0f, 100.0f ) );

	gc_MountEntity( &defaultECPS, btn0, btn1 );
	gc_MountEntity( &defaultECPS, btn1, btn2 );
	gc_MountEntity( &defaultECPS, btn1, btn3 );

	// serialize the buttons
	ecps_GenerateSerializedECPS( &defaultECPS, &serializedECPS );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &defaultECPS, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	SDL_assert( buttonCount == 4 );

	// destroy all buttons
	gp_DeleteAllOfGroup( &defaultECPS, 1 );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &defaultECPS, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	SDL_assert( buttonCount == 0 );

	// try saving out and loading in
	ecps_SaveSerializedECPS( "test.pkg", &serializedECPS );
	ecps_CleanSerialized( &serializedECPS );
	//*/
	ecps_LoadSerializedECPS( "test.pkg", &serializedECPS );

	// add some more buttons
	EntityID btn4 = createTestButton( vec2( 200.0f, 300.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn5 = createTestButton( vec2( 175.0f, 275.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn6 = createTestButton( vec2( 150.0f, 250.0f ), vec2( 100.0f, 100.0f ) );
	EntityID btn7 = createTestButton( vec2( 125.0f, 225.0f ), vec2( 100.0f, 100.0f ) );

	// deserialize the buttons
	ecps_CreateEntitiesFromSerializedComponents( &defaultECPS, &serializedECPS );
	
	// clean up
	ecps_CleanSerialized( &serializedECPS );

	// make sure the number of buttons is correct
	ecps_RunCustomProcess( &defaultECPS, countButtonsStart, countButtons, NULL, 1, gcGroupIDCompID );
	SDL_assert( buttonCount == 8 );//*/
}

static void testPointerResponseScreen_Exit( void )
{
}

static void testPointerResponseScreen_ProcessEvents( SDL_Event* e )
{
	//gp_PointerResponseEventHandler( e );
}

static void testPointerResponseScreen_Process( void )
{
	//ecps_RunProcess( &ecps, &gpPointerResponseProc );
}

static void testPointerResponseScreen_Draw( void )
{
	//ecps_RunProcess( &ecps, &gpRenderProc );
}

static void testPointerResponseScreen_PhysicsTick( float dt )
{
}

static void testPointerResponseScreen_Render( float normRenderTime )
{
	//ecps_RunCustomProcess( &defaultECPS, NULL, rotateButtons, NULL, 1, gcTransformCompID );
}

GameState testPointerResponseScreenState = { testPointerResponseScreen_Enter, testPointerResponseScreen_Exit, testPointerResponseScreen_ProcessEvents,
	testPointerResponseScreen_Process, testPointerResponseScreen_Draw, testPointerResponseScreen_PhysicsTick, testPointerResponseScreen_Render };