#include "helpers.h"

#if defined( WIN32 )
#include <Windows.h>
#include <shellapi.h>
#elif defined( __ANDROID__ )
#endif

#include <SDL.h>

#include "Math/vector2.h"
#include "Math/mathUtil.h"
#include "Input/input.h"
#include "System/platformLog.h"
#include "System/memory.h"

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

void printCash( char* string, size_t maxLen, int cash )
{
	bool printNegative = cash < 0;
	cash = SDL_abs( cash );
	int maxMod = divisorForDigitExtractionI32( cash );
	size_t pos = 0;
	size_t nextComma = digitsInI32( cash ) % 3;
	bool anyNumberPrinted = false;
	bool printedDollarSign = false;
	while( ( pos < maxLen - 1 ) && ( maxMod > 0 ) ) {
		if( printNegative ) {
			string[pos] = '-';
			printNegative = false;
		} else if( !printedDollarSign ) {
			string[pos] = '$';
			printedDollarSign = true;
		} else if( ( nextComma == 0 ) && anyNumberPrinted ) {
			string[pos] = ',';
			nextComma = 3;
		} else {
			string[pos] = '0' + (char)( cash / maxMod );
			cash = cash % maxMod;
			maxMod /= 10;
			nextComma = ( nextComma == 0 ) ? 2 : ( nextComma - 1 );
			anyNumberPrinted = true;
		}
		++pos;
	}

	// null terminate
	string[pos] = 0;
}

char* createStringCopy( const char* str )
{
	size_t len = SDL_strlen( str ) + 1;
	char* newStr = mem_Allocate( len );
	SDL_assert( newStr != NULL );
	if( newStr != NULL ) {
		SDL_strlcpy( newStr, str, len );
	}
	return newStr;
}

size_t strlenNullTest( const char* str )
{
	if( str == NULL ) {
		return 0;
	}

	return SDL_strlen( str );
}