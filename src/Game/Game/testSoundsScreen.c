#include "testSoundsScreen.h"

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "UI/text.h"
#include "sound.h"
#include "UI/uiEntities.h"
#include "DefaultECPS/defaultECPS.h"
#include "DefaultECPS/generalProcesses.h"

int testSnd = -1;
int testStrm = -1;

int font = -1;

void PlaySound( ECPS* ecps, Entity* btn )
{
	snd_Play( testSnd, 1.0f, 1.0f, 0.0f, 0 );
}

void ToggleStream( ECPS* ecps, Entity* btn )
{
	if( snd_IsStreamPlaying( testStrm ) ) {
		snd_StopStreaming( testStrm );
	} else {
		snd_PlayStreaming( testStrm, 1.0f, 0.0f, 0 );
	}
}

#define BUTTON_GROUP 1

static void testSoundsScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	gfx_SetClearColor( CLR_BLACK );

	testSnd = snd_LoadSample( "Sounds/test.ogg", 1, false );
	testStrm = snd_LoadStreaming( "Sounds/NowhereLand.ogg", 1, 0 );

	snd_SetMasterVolume( 0.25f );

	font = txt_LoadFont( "Fonts/Aileron-Regular.otf", 32 );

	EntityID playButton = button_CreateTextButton( &defaultECPS, vec2( 100.0f, 100.0f ), vec2( 100.0f, 100.0f ), "Play Sound",
		font, 32.0f, CLR_BLUE, VEC2_ZERO, 1, 0, NULL, PlaySound );
	gp_AddGroupIDToEntityAndChildren( &defaultECPS, playButton, BUTTON_GROUP );

	EntityID streamButton = button_CreateTextButton( &defaultECPS, vec2( 400.0f, 100.0f ), vec2( 100.0f, 100.0f ), "Stream",
		font, 32.0f, CLR_BLUE, VEC2_ZERO, 1, 0, NULL, ToggleStream );
	gp_AddGroupIDToEntityAndChildren( &defaultECPS, streamButton, BUTTON_GROUP );
}

static void testSoundsScreen_Exit( void )
{
	gp_DeleteAllOfGroup( &defaultECPS, BUTTON_GROUP );
	snd_UnloadSample( testSnd );
	snd_UnloadStream( testStrm );
}

static void testSoundsScreen_ProcessEvents( SDL_Event* e )
{
}

static void testSoundsScreen_Process( void )
{
}

static void testSoundsScreen_Draw( void )
{
}

static void testSoundsScreen_PhysicsTick( float dt )
{
}

GameState testSoundsScreenState = { testSoundsScreen_Enter, testSoundsScreen_Exit, testSoundsScreen_ProcessEvents,
	testSoundsScreen_Process, testSoundsScreen_Draw, testSoundsScreen_PhysicsTick };
