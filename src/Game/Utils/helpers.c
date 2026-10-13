#define STB_DS_IMPLEMENTATION

#include "helpers.h"

#if defined( WIN32 )
#include <Windows.h>
#include <shellapi.h>
#elif defined( __ANDROID__ )
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_assert.h>

#include "Math/vector2.h"
#include "Math/mathUtil.h"
#include "Input/input.h"
#include "System/platformLog.h"
#include "System/memory.h"
#include "System/random.h"
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
	if( str == NULL ) return NULL;

	size_t len = SDL_strlen( str ) + 1;
	char* newStr = mem_Allocate( len );
	ASSERT( newStr != NULL );
	if( newStr != NULL ) {
		SDL_strlcpy( newStr, str, len );
	}
	return newStr;
}

char* createStretchyStringCopy( const char* str )
{
	if( str == NULL ) return NULL;

	size_t len = SDL_strlen( str ) + 1;
	char* newStr = NULL;
	sb_Add( newStr, len );
	ASSERT( newStr != NULL );
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

bool compareOnlyFileName( const char* filePathOne, const char* filePathTwo )
{
	const char* startOne = SDL_strrchr( filePathOne, '/' );
	if( startOne == NULL ) {
		startOne = filePathOne;
	}
	const char* startTwo = SDL_strrchr( filePathTwo, '/' );
	if( startTwo == NULL ) {
		startTwo = filePathTwo;
	}

	return SDL_strcmp( startOne, startTwo ) == 0;
}

char* extractFileName( const char* filePath )
{
	char* file = NULL;

	const char* lastSlash = SDL_strrchr( filePath, '/' );
	const char* lastPeriod = SDL_strrchr( filePath, '.' );

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
	SDL_IOStream* ioStream = (SDL_IOStream*)( ctx->buf );
	if( SDL_ReadIO( ioStream, data, limit ) != limit ) {
		llog( LOG_ERROR, "Error reading from file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

static bool rwopsFileSkipper( struct cmp_ctx_s* ctx, size_t count )
{
	SDL_IOStream* ioStream = (SDL_IOStream*)( ctx->buf );
	if( SDL_SeekIO( ioStream, (Sint64)count, SDL_IO_SEEK_CUR ) == -1 ) {
		llog( LOG_ERROR, "Error seeking in file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

static size_t rwopsFileWriter( struct cmp_ctx_s* ctx, const void* data, size_t count )
{
	SDL_IOStream* ioStream = (SDL_IOStream*)( ctx->buf );
	if( SDL_WriteIO( ioStream, data, count ) != count ) {
		llog( LOG_ERROR, "Error writing to file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

SDL_IOStream* openRWopsCMPFile( const char* filePath, const char* mode, cmp_ctx_t* cmpCtx )
{
	ASSERT_AND_IF_NOT( filePath != NULL ) return NULL;
	ASSERT_AND_IF_NOT( mode != NULL ) return NULL;
	ASSERT_AND_IF_NOT( cmpCtx != NULL ) return NULL;

	// TODO: handle not overwriting an existing file if the writing fails
	SDL_IOStream* ioStream = SDL_IOFromFile( filePath, mode );
	if( ioStream == NULL ) {
		llog( LOG_ERROR, "Unable to open file %s: %s", filePath, SDL_GetError( ) );
		return NULL;
	}

	cmp_init( cmpCtx, ioStream, rwopsFileReader, rwopsFileSkipper, rwopsFileWriter );

	return ioStream;
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

xtUUID createRandomUUID( )
{
	xtUUID uuid;

	for( int i = 0; i < 16; ++i ) {
		uuid.parts[i] = rand_GetU8( NULL );

		// UUID version 4 identifier
		if( i == 6 ) uuid.parts[i] = ( uuid.parts[i] & 0x0F ) | 0x40;

		// variant 1
		if( i == 8 ) uuid.parts[i] = ( uuid.parts[i] & 0x3F ) | 0x80;
	}

	return uuid;
}

char* printUUID( const xtUUID* uuid )
{
	char* str = mem_Allocate( sizeof( char ) * 37 );
	SDL_snprintf( str, 37, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		uuid->parts[0], uuid->parts[1], uuid->parts[2], uuid->parts[3], uuid->parts[4], uuid->parts[5],
		uuid->parts[6], uuid->parts[7], uuid->parts[8], uuid->parts[9], uuid->parts[10], uuid->parts[11],
		uuid->parts[12], uuid->parts[13], uuid->parts[14], uuid->parts[15] );
	return str;
}

void testPathFunctions( void )
{
	const char* fromAbsolutePath = "C:\\testing/test.dot\\andMore//evenMore";
	const char* toAbsolutePath = "C:\\testing\\test.dot\\branchback\\somemore//file.txt";
	const char* otherToAbsolutePath = "C:\\testing/test.dot\\andMore//evenMore\\other.txt";

	char* sbRelativePath = getRelativePathSB( fromAbsolutePath, toAbsolutePath, false );
	char* sbOtherRelativePath = getRelativePathSB( fromAbsolutePath, otherToAbsolutePath, false );
	ASSERT( SDL_strcmp( sbRelativePath, ".\\..\\..\\branchback\\somemore\\file.txt" ) == 0 );
	ASSERT( SDL_strcmp( sbOtherRelativePath, ".\\other.txt" ) == 0 );

	char* sbOrigAbsolutePath = getAbsolutePathSB( fromAbsolutePath, sbRelativePath, false );
	char* sbOtherAbsolutePath = getAbsolutePathSB( fromAbsolutePath, sbOtherRelativePath, false );
	ASSERT( SDL_strcmp( sbOrigAbsolutePath, "C:\\testing\\test.dot\\branchback\\somemore\\file.txt" ) == 0 );
	ASSERT( SDL_strcmp( sbOtherAbsolutePath, "C:\\testing\\test.dot\\andMore\\evenMore\\other.txt" ) == 0 );

	const char* fromAbsolutePathWFile = "C:\\testing/test.dot\\andMore//evenMore\\cutOffFile.txt";
	char* sbOrigAbsolutePathWFile = getAbsolutePathSB( fromAbsolutePathWFile, sbRelativePath, true );
	char* sbOtherAbsolutePathWFile = getAbsolutePathSB( fromAbsolutePathWFile, sbOtherRelativePath, true );
	ASSERT( SDL_strcmp( sbOrigAbsolutePathWFile, "C:\\testing\\test.dot\\branchback\\somemore\\file.txt" ) == 0 );
	ASSERT( SDL_strcmp( sbOtherAbsolutePathWFile, "C:\\testing\\test.dot\\andMore\\evenMore\\other.txt" ) == 0 );

	const char* fromSameDirPath = "C:\\testing\\more\\fromFile.txt";
	const char* toSameDirPath = "C:\\testing\\more\\toFile.txt";
	char* sbSameDirPath = getRelativePathSB( fromSameDirPath, toSameDirPath, true );
	ASSERT( SDL_strcmp( sbSameDirPath, ".\\toFile.txt" ) == 0 );

	sb_Release( sbRelativePath );
	sb_Release( sbOtherRelativePath );
	sb_Release( sbOrigAbsolutePath );
	sb_Release( sbOtherAbsolutePath );
}

// used to represent a substring of an existing string
typedef struct {
	const char* string;
	size_t len;
} StringSpan;

static void appendStringSpanSB( char** sbBaseString, StringSpan span )
{
	ASSERT_AND_IF_NOT( sbBaseString != NULL ) return;
	if( ( span.len == 0 ) || ( span.string == NULL ) ) return;

	char* sbString = *sbBaseString;
	if( sbString != NULL ) sb_Pop( sbString );

	char* start = sb_Add( sbString, span.len );
	memcpy( start, span.string, span.len );
	sb_Push( sbString, 0 );
	*sbBaseString = sbString;
}

// returns a stretchy buffer containing the full path
char* getAbsolutePathSB( const char* absoluteBase, const char* relative, bool ignoreAbsoluteEOS )
{
	// so we have the absolute base path, need to split it into separate parts, from there back track
	//  for each ../ in the relative path, then append the rest of the relative path to it

	// create the list of string spans representing the base path
	StringSpan* sbBasePathParts = NULL;
	size_t startPos = 0;
	for( size_t i = 0; i <= SDL_strlen( absoluteBase ); ++i ) {
		bool foundSpan = absoluteBase[i] == '\\';
		foundSpan = foundSpan || ( absoluteBase[i] == '/' );
		foundSpan = foundSpan || ( ( absoluteBase[i] == 0 ) && !ignoreAbsoluteEOS );
		if( foundSpan ) {
			size_t len = i - startPos;
			if( len > 0 ) {
				StringSpan newSpan = { .string = &absoluteBase[i - len], .len = len };
				sb_Push( sbBasePathParts, newSpan );
			}
			startPos = i + 1;
		}
	}
	size_t len = sb_Count( sbBasePathParts );

	// create the list of string spans representing the relative path
	startPos = 0;
	for( size_t i = 0; i <= SDL_strlen( relative ); ++i ) {
		bool foundSpan = relative[i] == '\\';
		foundSpan = foundSpan || ( relative[i] == '/' );
		foundSpan = foundSpan || ( relative[i] == 0 );
		if( foundSpan ) {
			size_t len = i - startPos;
			if( len > 0 ) {
				const char* string = &relative[i - len];
				if( ( len == 2 ) && ( SDL_strncmp( "..", string, 2 ) == 0 ) ) {
					// go down a level
					sb_Pop( sbBasePathParts );
				} else if( ( len == 1 ) && ( SDL_strncmp( ".", string, 1 ) == 0 ) ) {
					// just ignore this
				} else {
					// push onto the path
					StringSpan newSpan = { .string = &relative[i - len], .len = len };
					sb_Push( sbBasePathParts, newSpan );
				}
			}
			startPos = i + 1;
		}
	}

	// build the final path
	char* sbRetPath = NULL;
	for( size_t i = 0; i < sb_Count( sbBasePathParts ); ++i ) {
		if( sbRetPath != NULL ) {
			sb_Insert( sbRetPath, sb_Count( sbRetPath ) - 2, '\\' );
		}
		appendStringSpanSB( &sbRetPath, sbBasePathParts[i] );
	}

	sb_Release( sbBasePathParts );

	return sbRetPath;
}

// returns a stretchy buffer containing the relative path
//  from is expected to be a directory path
// NOTE: this assumes you're passing in full paths
char* getRelativePathSB( const char* from, const char* to, bool fromIsFile )
{
	ASSERT_AND_IF_NOT( from != NULL ) return NULL;
	ASSERT_AND_IF_NOT( to != NULL ) return NULL;

	if( from[0] != to[0] ) {
		// different drives, so just copy the to path into the result
		return createStretchyStringCopy( to );
	}

	// create local copies of strings so we can tokenize them
	char* sbLocalFrom = createStretchyStringCopy( from );
	char* sbLocalTo = createStretchyStringCopy( to );

	const char* tokens = "/\\";

	char* fromTokSave = NULL;
	char* fromToken = SDL_strtok_r( sbLocalFrom, tokens, &fromTokSave );

	char* toTokSave = NULL;
	char* toToken = SDL_strtok_r( sbLocalTo, tokens, &toTokSave );

	// loop through until we find a spot where the tokens don't match
	bool tokensMatch = ( strcmp( fromToken, toToken ) == 0 );
	while( ( fromToken != NULL ) && ( toToken != NULL ) && tokensMatch ) {
		fromToken = SDL_strtok_r( NULL, tokens, &fromTokSave );
		toToken = SDL_strtok_r( NULL, tokens, &toTokSave );

		if( ( fromToken != NULL ) && ( toToken != NULL ) ) {
			tokensMatch = ( strcmp( fromToken, toToken ) == 0 );
		}
	}

	// now we want to start building the actual string we'll return
	// go down the rest of the from path
	char sep = '\\';
	char* sbFinalPath = NULL;

	sb_Push( sbFinalPath, '.' );
	sb_Push( sbFinalPath, sep );

	if( fromIsFile && ( fromToken != NULL ) ) {
		// cut of the one extra previous token we'd get if we parsed the file
		fromToken = SDL_strtok_r( NULL, tokens, &fromTokSave );
	}

	while( fromToken != NULL ) {
		fromToken = SDL_strtok_r( NULL, tokens, &fromTokSave );
		sb_Push( sbFinalPath, '.' );
		sb_Push( sbFinalPath, '.' );
		sb_Push( sbFinalPath, sep );
	}

	// grab the rest of the tokens from sbFilePath and append them to a new string
	while( toToken != NULL ) {
		if( ( sbFinalPath != NULL ) && ( sb_Last( sbFinalPath ) != sep ) ) sb_Push( sbFinalPath, sep );
		size_t tokenLen = strlen( toToken );
		char* strAddSpot = sb_Add( sbFinalPath, tokenLen );
		memcpy( strAddSpot, toToken, sizeof( char ) * tokenLen );

		toToken = SDL_strtok_r( NULL, tokens, &toTokSave );
	}
	// add terminating null
	sb_Push( sbFinalPath, 0 );

	sb_Release( sbLocalFrom );
	sb_Release( sbLocalTo );

	return sbFinalPath;
}