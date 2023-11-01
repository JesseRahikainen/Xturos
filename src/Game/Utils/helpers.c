#include "helpers.h"

#if defined( WIN32 )
#include <Windows.h>
#include <shellapi.h>
#elif defined( __ANDROID__ )
#endif

#include <SDL.h>
#include <SDL_assert.h>

#include "Math/vector2.h"
#include "Math/mathUtil.h"
#include "Input/input.h"
#include "System/platformLog.h"
#include "System/memory.h"
#include "Utils/stretchyBuffer.h"

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

char* createStretchyStringCopy( const char* str )
{
	size_t len = SDL_strlen( str ) + 1;
	char* newStr = NULL;
	sb_Add( newStr, len );
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

char* extractFileName( const char* filePath )
{
	char* file = NULL;

	const char* lastSlash = strrchr( filePath, '/' );
	const char* lastPeriod = strrchr( filePath, '.' );

	if( lastSlash == NULL ) {
		lastSlash = filePath;
	} else {
		++lastSlash;
	}

	if( lastPeriod == NULL ) {
		size_t filePathLen = strlen( filePath );
		lastPeriod = filePath + filePathLen;
	}

	size_t size = 0;
	const char* curr = lastSlash;
	while( curr++ != lastPeriod ) {
		++size;
	}

	if( size <= 0 ) {
		return NULL;
	}

	file = mem_Allocate( sizeof( char ) * ( size + 1 ) );
	strncpy( file, lastSlash, size );
	file[size] = 0;

	return file;
}

#ifdef WIN32
// wideStr needs to be null terminated, returns a stretchy buffer
char* wideCharToUTF8SB( const wchar_t* wideStr )
{
	int sizeUTF8 = WideCharToMultiByte( CP_UTF8, 0, wideStr, -1, NULL, 0, NULL, NULL );
	char* str = NULL;
	sb_Add( str, sizeUTF8 + 1 );
	WideCharToMultiByte( CP_UTF8, 0, wideStr, -1, str, sizeUTF8, NULL, NULL );
	return str;
}
#endif

static bool rwopsFileReader( struct cmp_ctx_s* ctx, void* data, size_t limit )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWread( rwops, data, limit, 1 ) != 1 ) {
		llog( LOG_ERROR, "Error reading from file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

static bool rwopsFileSkipper( struct cmp_ctx_s* ctx, size_t count )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWseek( rwops, (Sint64)count, RW_SEEK_CUR ) == -1 ) {
		llog( LOG_ERROR, "Error seeking in file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

static size_t rwopsFileWriter( struct cmp_ctx_s* ctx, const void* data, size_t count )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWwrite( rwops, data, count, 1 ) != 1 ) {
		llog( LOG_ERROR, "Error writing to file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

SDL_RWops* openRWopsCMPFile( const char* filePath, const char* mode, cmp_ctx_t* cmpCtx )
{
	ASSERT_AND_IF( filePath != NULL ) return NULL;
	ASSERT_AND_IF( mode != NULL ) return NULL;
	ASSERT_AND_IF( cmpCtx != NULL ) return NULL;

	// TODO: handle not overwriting an existing file if the writing fails
	SDL_RWops* rwopsFile = SDL_RWFromFile( filePath, mode );
	if( rwopsFile == NULL ) {
		llog( LOG_ERROR, "Unable to open file %s: %s", filePath, SDL_GetError( ) );
		return NULL;
	}

	cmp_init( cmpCtx, rwopsFile, rwopsFileReader, rwopsFileSkipper, rwopsFileWriter );

	return rwopsFile;
}

int nextHighestPowerOfTwo( int v )
{
	v += ( v == 0 ); // fixes case where v == 0 returns 0

	//v--;
	// fill everything to the right of the highest set bit with 1
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	// add 1 to get the next highest power of 2
	v++;

	return v;
}