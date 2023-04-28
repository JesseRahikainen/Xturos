#include "gameProcesses.h"

#include <SDL_assert.h>

#include "gameComponents.h"
#include "Components\generalComponents.h"
#include "Processes\generalProcesses.h"
#include "System\ECPS\entityComponentProcessSystem.h"

void gameProcesses_Setup( ECPS* ecps )
{
	SDL_assert( ecps != NULL );

	gp_RegisterProcesses( ecps );

	// register rest of game specific processes here
}