#include "helpers.h"

#include <SDL_filesystem.h>

#include "../Math/vector2.h"
#include "../Input/input.h"
#include "../System/platformLog.h"
#include "../System/memory.h"

void logMousePos( void )
{
	Vector2 pos;
	input_GetMousePosition( &pos );
	vec2_Dump( &pos, "Mouse pos" );
}

char* getSavePath( char* fileName )
{
	char* path = SDL_GetPrefPath( "Company Name", "Game Name" );
	size_t len = strlen( path ) + strlen( fileName ) + 2;
	char* fullPath = mem_Allocate( sizeof( char ) * len );
	fullPath[0] = 0;
	strcat( fullPath, path );
	strcat( fullPath, fileName );

	SDL_free( path );

	return fullPath;
}