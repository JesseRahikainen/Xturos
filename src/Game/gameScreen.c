#include "gameScreen.h"

static int gameScreen_Enter( void )
{
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
}

static void gameScreen_PhysicsTick( float dt )
{
}

struct GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };