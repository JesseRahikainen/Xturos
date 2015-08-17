#ifndef STRETCH_BUFFER_H
#define STRETCH_BUFFER_H

#include <assert.h>
#include "../System/memory.h"

// this is basically the stb library stretchy buffer modified to use our memory manager
//  assume the pointer used is what we'll use, before the pointer are two ints, one of the last used, and one for the allocated size
//  (totalSize, useSize, [data])
#define sb__Raw( p )	( ( (size_t*)(p) ) - 2 )
#define sb__Total( p )	( sb__Raw( p )[0] )
#define sb__Used( p )	( sb__Raw( p )[1] )

#define sb__NeedGrow( p, g )	( ( (p) == 0 ) || ( ( sb__Used( (p) ) + (g) ) >= sb__Total( p ) ) )
#define sb__TestAndGrow( p, c )	( sb__NeedGrow( p, (c) ) ? ( (p) = sb__GrowData( (p), (c), sizeof( (p)[0] ) ) ) : 0 )

#define sb_Release( p )	( (p) ? ( mem_Release( sb__Raw( (p) ) ), 0 ) : 0 )
#define sb_Push( p, v )	( sb__TestAndGrow( (p), 1 ), (p)[sb__Used(p)++] = (v) )
#define sb_Pop( p, t )		( --sb__Used(p), ( sb__Used(p) >= 0 ) ? (t)( (p)[sb__Used(p)] ) : ( assert( "Nothing to pop" ), (t)0 ) )
#define sb_Count( p )	( (p) ? sb__Used( p ) : 0 )
#define sb_Add( p, g )	( sb__TestAndGrow( (p), (g) ), sb__Used( (p) ) += (g), &(p)[sb__Used((p)) - (g)] )

static void* sb__GrowData( void* p, int increment, size_t itemSize )
{
	int currSize = p ? sb__Total( p ) : 0;
	int currBased = currSize + ( currSize / 2 ); // 1.5 * current
	int min = currSize + increment;
	int newCount = ( min > currBased ) ? min : currBased;
	int* np = mem_Resize( p ? (void*)( sb__Raw(p) ) : NULL, ( newCount * itemSize ) + ( sizeof( int) * 2 ) );
	if( np != NULL ) {
		if( p == NULL ) {
			np[1] = 0;
		}
		np[0] = newCount;
		return np+2;
	} else {
		assert( "Error allocating stretchy array." );
		return p;
	}
}

/*
static void sb_Test( void )
{
	//SDL_Log( SDL_LOG_CATEGORY_APPLICATION, txt );
	printf( "-- Testing stretchy buffer --\n" );

#define DUMP_ARRAY( a ) \
	if( a == NULL ) { printf( "!!! STRETCHY BUFFER DOES NOT EXIST !!!\n" ); } \
	else { \
	    printf( "Count: %i   Total: %i   Used: %i\n", sb_Count( a ), sb__Total( a ), sb__Used( a ) ); \
		for( int i = 0; i < sb__Used( a ); ++i ) { printf( "  %i: %f\n", i, a[i] ); } \
	}

	float* testArray = NULL;

	sb_Push( testArray, 0.0f );
	DUMP_ARRAY( testArray );

	sb_Push( testArray, 1.0f );
	DUMP_ARRAY( testArray );

	float* start = sb_Add( testArray, 5 );
	for( int i = 0; i < 5; ++i ) {
		start[i] = (float)( 2 + i );
	}
	DUMP_ARRAY( testArray );

	start = sb_Add( testArray, 1 );
	start[0] = 8.0f;
	DUMP_ARRAY( testArray );

	start = sb_Add( testArray, 1 );
	start[0] = 9.0f;
	DUMP_ARRAY( testArray );

	start = sb_Add( testArray, 1 );
	start[0] = 10.0f;
	DUMP_ARRAY( testArray );

	start = sb_Add( testArray, 1 );
	start[0] = 11.0f;
	DUMP_ARRAY( testArray );

	while( sb_Count( testArray ) > 0 ) {
		float val = sb_Pop( testArray, float );
		printf( "Popped: %f\n", val );
		DUMP_ARRAY( testArray );
	}

clean_up:
	testArray = (float*)( sb_Release( testArray ) );
	if( testArray == NULL ) {
		printf( "Release stretchy buffer successful\n" );
	} else {
		printf( "!!! RELEASE STRETCHY BUFFER UNSUCCESSFUL !!!\n" );
	}

	printf( "-- End testing stretchy buffer --\n" );
}
*/

#endif // inclusion guard