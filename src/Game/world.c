#include "world.h"

#include <SDL_assert.h>
#include "stdlib.h"

static int worldWidth;
static int worldHeight;

void world_SetSize( int width, int height )
{
	worldWidth = width;
	worldHeight = height;
}

void world_GetSize( Vector2* outVec )
{
	SDL_assert( outVec != NULL );

	outVec->x = (float)worldWidth;
	outVec->y = (float)worldHeight;
}