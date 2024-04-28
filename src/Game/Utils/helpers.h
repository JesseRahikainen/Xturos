#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>
#include <stddef.h>

#include <SDL.h>

#include "Others/cmp.h"

// general helper functions
#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[0] ) )
#define ANY_BITS_ON( bits, testOnBits ) ( ( bits ) & ( testOnBits ) )
#define TURN_ON_BITS( currBits, onBits ) ( ( currBits ) |= ( onBits ) )
#define TURN_OFF_BITS( currBits, offBits ) ( ( currBits ) &= ~( offBits ) )
#define TOGGLE_BITS( currBits, toggleBits ) ( ( currBits ) ^= ( toggleBits ) )

// copied from nuklear.h, don't want to have to rely on that
#if defined( _MSC_VER )
#define ALIGN_OF( t ) ( __alignof( t ) )
#else
// creates a structure with a byte and the desired type, casts 0 to it, then finds the difference between the position of the desired type and 0
#define ALIGN_OF( t ) ( (char*)( &( (struct { char c; t _h; }*)0 )->_h ) - (char*)0 )
#endif

// allow warning messages on MSVC
#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define WARNING(desc) message(__FILE__ "(" STRINGIZE(__LINE__) ") : Warning: " #desc)

// https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

void logMousePos( void );
char* getSavePath( char* fileName );

void printCash( char* string, size_t maxLen, int32_t cash );

// creates a copy of the string on the heap, memory will have to be released manually
char* createStringCopy( const char* str );

// creates a copy of the string into a stretchy buffer, will have be cleaned up with sb_Release
char* createStretchyStringCopy( const char* str );

// does a null test on str and returns 0 if it's NULL
size_t strlenNullTest( const char* str );

// useful when wanting to test and early out if the test fails
#define ASSERT_AND_IF_NOT( x ) SDL_assert( x ); if( !( x ) )

// useful when wanting to test and do something if the test passes
#define ASSERT_AND_IF( x ) SDL_assert( x ); if( ( x ) )

char* extractFileName( const char* filePath );

#if defined( WIN32 )
#include <wchar.h>
// wideStr needs to be null terminated, returns a stretchy buffer
char* wideCharToUTF8SB( const wchar_t* wideStr );
#endif

SDL_RWops* openRWopsCMPFile( const char* filePath, const char* mode, cmp_ctx_t * cmpCtx );

#define CMP_WRITE( cmpPtr, val, write, type, desc, onFail ) \
	if( !write( (cmpPtr), (val) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for %s: %s", (desc), (type), cmp_strerror( cmpPtr ) ); \
		onFail; }

#define CMP_WRITE_STR( cmpPtr, val, type, desc, onFail ) \
	if( !cmp_write_str( (cmpPtr), (val), (uint32_t)SDL_strlen((val)) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for %s: %s", (desc), (type), cmp_strerror( cmpPtr ) ); \
		onFail; }

#define CMP_READ( cmpPtr, val, read, type, desc, onFail ) \
	if( !read( (cmpPtr), &(val) ) ) { \
		llog( LOG_ERROR, "Unable to read %s for %s: %s", (desc), (type), cmp_strerror( cmpPtr ) ); \
		onFail; }

#define CMP_READ_STR( cmpPtr, val, bufferSize, type, desc, onFail ) \
	if( !cmp_read_str( (cmpPtr), (val), &(bufferSize) ) ) { \
		llog( LOG_ERROR, "Unable to read %s for %s: %s", (desc), (type), cmp_strerror( cmpPtr ) ); \
		onFail; }

int nextHighestPowerOfTwo( int v );

typedef struct {
	uint8_t parts[16];
} xtUUID;
xtUUID createRandomUUID( );
char* printUUID( const xtUUID* uuid );


#endif /* inclusion guard */