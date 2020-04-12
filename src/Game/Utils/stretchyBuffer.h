#ifndef STRETCH_BUFFER_H
#define STRETCH_BUFFER_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../System/memory.h"

// this is basically the stb library stretchy buffer modified to use our memory manager
//  assume the pointer used is what we'll use, before the pointer are two size_ts, one for the last used, and one for the allocated size
//  (totalSize, useSize, [data])
#define sb__Raw( ptr )	( ( (size_t*)(ptr) ) - 2 )
#define sb__Total( ptr )	( sb__Raw( ptr )[0] )
#define sb__Used( ptr )	( sb__Raw( ptr )[1] )

#define sb__NeedGrow( ptr, amt )	( ( (ptr) == 0 ) || ( ( sb__Used( (ptr) ) + (amt) ) >= sb__Total( ptr ) ) )
#define sb__TestAndGrow( ptr, cnt )	( sb__NeedGrow( ptr, (cnt) ) ? ( (ptr) = sb__GrowData( (ptr), (cnt), sizeof( (ptr)[0] ), __FILE__, __LINE__ ) ) : 0 )

// releases all memory in use by the buffer and sets the pointer to null
#define sb_Release( ptr )	( (ptr) ? ( mem_Release( sb__Raw( (ptr) ) ), ptr = 0, 0 ) : 0 )

// Pushes the value onto the end of the buffer
#define sb_Push( ptr, val )	( sb__TestAndGrow( (ptr), 1 ), (ptr)[sb__Used(ptr)++] = (val) )

// reduces the number of elements in the buffer by 1, and returns the value in the spot that was removed, will cause issues if the size is zero
#define sb_Pop( ptr )	( --sb__Used(ptr), (ptr)[sb__Used(ptr)] )
//#define sb_Pop( ptr, type )	( --sb__Used(ptr), ( sb__Used(ptr) >= 0 ) ? (type)( (ptr)[sb__Used(ptr)] ) : ( assert( "Nothing to pop" ), (type)0 ) )

// returns the number of elements in the buffer that are currently in use
#define sb_Count( ptr )	( (ptr) ? sb__Used( ptr ) : 0 )

// adds a specific number of elements to the buffer and gives you the address where the first of the new elements was added
#define sb_Add( ptr, amt )	( sb__TestAndGrow( (ptr), (amt) ), sb__Used( (ptr) ) += (amt), &(ptr)[sb__Used((ptr)) - (amt)] )

// returns the data in the last spot in the array
#define sb_Last( ptr )	( (ptr)[ sb__Used( (ptr) ) - 1] )

// sets all the memory in the stretchy buffer as unused, doesn't deallocate memory
#define sb_Clear( ptr ) ( (ptr) ? ( sb__Used( ptr ) = 0 ) : 0 )

// if you try to insert at a value past the end of the array it acts as a push
#define sb_Insert( ptr, at, val ) ( ( (at) >= sb_Count( (ptr) ) ) ? sb_Push( (ptr), (val) ) : ( sb_Add( (ptr), 1 ), memmove( (ptr)+(at)+1, (ptr)+(at), sizeof( (ptr)[0] ) * ( sb_Count( (ptr) ) - (at) - 1 ) ), (ptr)[(at)] = (val) ) )

// if you try to remove past the end of the array nothing is removed
#define sb_Remove( ptr, at ) ( (at) < sb_Count( (ptr) ) ? ( memmove( (ptr)+(at), (ptr)+(at)+1, sizeof( (ptr)[0] ) * ( sb__Used( ptr ) - ( (at) + 1 ) ) ), --sb__Used( (ptr) ) ) : 0 )

// increases the amount of allocated space by amt
#define sb_Reserve( ptr, amt ) ( ( ( (ptr) == 0 ) || ( sb__Total( ptr ) < amt ) ) ? ( (ptr) = sb__GrowData( (ptr), (amt), sizeof( (ptr)[0] ), __FILE__, __LINE__ ) ) : 0 )

// returns the amount of total space reserved for the buffer
#define sb_Reserved( ptr ) ( (ptr) ? sb__Total( (ptr) ) : 0 )

static void* sb__GrowData( void* p, int increment, size_t itemSize, const char* fileName, const int fileLine )
{
	int currSize = p ? sb__Total( p ) : 0;
	int currBased = currSize + ( currSize / 2 ); // 1.5 * current
	int min = currSize + increment;
	int newCount = ( min > currBased ) ? min : currBased;
	int* np = mem_Resize_Data( p ? (void*)( sb__Raw(p) ) : NULL, ( newCount * itemSize ) + ( sizeof( int) * 2 ), fileName, fileLine );
	//int* np = mem_Resize( p ? (void*)( sb__Raw(p) ) : NULL, ( newCount * itemSize ) + ( sizeof( int) * 2 ) );
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
	//llog( LOG_DEBUG,  SDL_LOG_CATEGORY_APPLICATION, txt );
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

	// test insert at beginning
	sb_Insert( testArray, 12.0f, 0 );
	DUMP_ARRAY( testArray );

	// test insert past end
	sb_Insert( testArray, 13.0f, 10000 );
	DUMP_ARRAY( testArray );

	// test insert in middle
	sb_Insert( testArray, 14.0f, 

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