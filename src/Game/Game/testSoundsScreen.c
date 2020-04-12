#include "gameScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../sound.h"
#include "../UI/button.h"

int testSnd = -1;
int testStrm = -1;

int font = -1;

void PlaySound( int id )
{
	snd_Play( testSnd, 1.0f, 1.0f, 0.0f, 0 );
}

void ToggleStream( int id )
{
	if( snd_IsStreamPlaying( testStrm ) ) {
		snd_StopStreaming( testStrm );
	} else {
		snd_PlayStreaming( testStrm, 1.0f, 0.0f );
	}
}

static int testSoundsScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	gfx_SetClearColor( CLR_BLACK );

	testSnd = snd_LoadSample( "Sounds/test.ogg", 1, false );
	testStrm = snd_LoadStreaming( "Sounds/NowhereLand.ogg", 1, 0 );

	snd_SetMasterVolume( 0.25f );

	font = txt_LoadFont( "Fonts/kenpixel.ttf", 32 );

	btn_Create( vec2( 100.0f, 100.0f ), vec2( 100.0f, 100.0f ), vec2( 100.0f, 100.0f ),
		"Play Sound", font, 32.0f, CLR_BLUE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0, NULL, PlaySound );

	btn_Create( vec2( 400.0f, 100.0f ), vec2( 100.0f, 100.0f ), vec2( 100.0f, 100.0f ),
		"Stream", font, 32.0f, CLR_BLUE, VEC2_ZERO, NULL, -1, CLR_WHITE, 1, 0, NULL, ToggleStream );

	btn_RegisterSystem( );

	return 1;
}

static int testSoundsScreen_Exit( void )
{
	btn_UnRegisterSystem( );

	snd_UnloadSample( testSnd );
	snd_UnloadStream( testStrm );

	return 1;
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