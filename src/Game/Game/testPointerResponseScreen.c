#include "testPointerResponseScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

#include "../System/ECPS/entityComponentProcessSystem.h"

#include "../Components/generalComponents.h"
#include "../Processes/generalProcesses.h"

static ECPS ecps;
static int whiteImg;
static Vector2 imgSize;

static void onButtonOver( Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_GREY;
}

static void onButtonLeave( Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_WHITE;
}

static void onButtonPress( Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_RED;
}

static void onButtonRelease( Entity* btn )
{
	GCColorData* clr;
	ecps_GetComponentFromEntity( btn, gcClrCompID, &clr );
	clr->futureClr = CLR_YELLOW;
}

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
	ptr.state = 0;

	GCSpriteData sprite;
	sprite.camFlags = 1;
	sprite.depth = 0;
	sprite.img = whiteImg;

	GCScaleData scale;
	scale.currScale.x = size.x / imgSize.x;
	scale.currScale.y = size.y / imgSize.y;
	scale.futureScale = scale.currScale;

	ecps_CreateEntity( &ecps, 5,
		gcPosCompID, &pos,
		gcClrCompID, &clr,
		gcPointerResponseCompID, &ptr,
		gcSpriteCompID, &sprite,
		gcScaleCompID, &scale );
}

static int testPointerResponseScreen_Enter( void )
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

	return 1;
}

static int testPointerResponseScreen_Exit( void )
{
	return 1;
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