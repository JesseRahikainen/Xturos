#include "gameScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"

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
	//img_Draw( spriteSheetImg, 1, pos[0], pos[0], 0 );
	for( int i = 0; i < numImagesOnSheet; ++i ) {
		img_Draw( imageSheet[i], 1, pos[i], pos[i], 0 );
	}
}

static void gameScreen_PhysicsTick( float dt )
{
}

struct GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };