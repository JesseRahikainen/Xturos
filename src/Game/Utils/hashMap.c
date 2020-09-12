#include "hashMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <SDL_stdinc.h>

#include "../Math/mathUtil.h"
#include "../System/memory.h"
#include "../System/platformLog.h"
#include "../Utils/helpers.h"

static uint32_t hashFunc_DJB2( const char* str )
{
	uint32_t hash = 5381;
	uint32_t c;

	c = *str++;
	while( c ) {
		hash = ( ( hash << 5 ) + hash ) + c; // ( hash * 33 ) + c
		c = *str++;
	}

	return hash;
}

static bool isPowerOfTwoU32( uint32_t val )
{
	return ( ( val == 1 ) || ( ( val & ( val - 1 ) ) == 0 ) );
}

// we're assuming denom is a power of two
static uint32_t powerTwoModulus( uint32_t num, uint32_t denom )
{
	assert( isPowerOfTwoU32( denom ) );
	return ( num & ( denom - 1 ) );
}

static uint32_t chooseCapacity( uint32_t baseSize )
{
	size_t size = baseSize - 1;
	size = size | ( size >> 1 );
	size = size | ( size >> 2 );
	size = size | ( size >> 4 );
	size = size | ( size >> 6 );
	size = size | ( size >> 16 );

	return ( size + 1 );
}

// wraps the modulus function, so it doesn't have to be in the hash function
static uint32_t hash( HashMap* hashMap, const char* key )
{
	return powerTwoModulus( hashMap->hashFunc( key ), hashMap->capacity );
}

#define NEXT_IDX( hm, curr ) ( ( ( (curr) + 1 ) >= ( (hm)->capacity ) ) ? 0 : ( (curr) + 1 ) )

void hashMap_Init( HashMap* hashMap, uint32_t estimatedSize, HashFunc hashFunc )
{
	//hashMap->hashFunc = hashFunc_Test;
	hashMap->keys = NULL;
	hashMap->values = NULL;
	hashMap->capacity = 0;

	hashMap->hashFunc = hashFunc_DJB2;
	if( hashFunc != NULL ) {
		hashMap->hashFunc = hashFunc;
	}

	if( estimatedSize == 0 ) {
		return;
	}

	// set an initial size based on the estimated size
	hashMap->capacity = chooseCapacity( estimatedSize * 2 );

	hashMap->keys = (HashMapKey*)mem_Allocate( sizeof( HashMapKey ) * hashMap->capacity );
	hashMap->values = (int*)mem_Allocate( sizeof( int ) * hashMap->capacity );

	for( size_t i = 0; i < hashMap->capacity; ++i ) {
		hashMap->keys[i].key = NULL;
		hashMap->keys[i].probeLength = UINT8_MAX;
	}

	// android doesn't support log2, so we just do ln( c ) / ln( 2 )
	//hashMap->probeLimit = (size_t)log2( (double)hashMap->capacity );
	hashMap->probeLimit = (size_t)( SDL_log( (double)hashMap->capacity ) * 1.4426950408889634 );
}

static void resize( HashMap* hashMap, uint32_t newSize )
{
	// allocate new list and move data
	HashMap newHashMap;
	hashMap_Init( &newHashMap, newSize, hashMap->hashFunc );

	// insert elements from old hashmap
	for( size_t i = 0; i < hashMap->capacity; ++i ) {
		if( hashMap->keys[i].probeLength != UINT8_MAX ) {
			hashMap_Set( &newHashMap, hashMap->keys[i].key, hashMap->values[i] );
			mem_Release( hashMap->keys[i].key );
		}
	}

	// release old data
	mem_Release( hashMap->keys );
	mem_Release( hashMap->values );

	// copy over new data
	(*hashMap) = newHashMap;
}

static bool matches( HashMap* hashMap, const char* key, int idx, uint8_t currLength )
{
	return ( ( hashMap->keys[idx].probeLength == currLength ) && ( strcmp( key, hashMap->keys[idx].key ) == 0 ) );
}

// returns whether the key was found, if it was outIdx will contain the index
static bool findIndex( HashMap* hashMap, const char* key, uint32_t* outIdx )
{
	uint32_t idx = hash( hashMap, key );
	uint8_t currLength = 0;

	while( ( hashMap->keys[idx].probeLength != UINT8_MAX ) && ( currLength <= hashMap->keys[idx].probeLength ) ) {

		// seeing if the probe lengths are equal is a good first pass fail thing
		if( matches( hashMap, key, idx, currLength ) ) {
			(*outIdx) = idx;
			return true;
		}

		idx = NEXT_IDX( hashMap, idx );
		++currLength;
	}

	return false;
}

#define SWAP( l, r, type ) { type swap = (type)l; l = (type)r; r = swap; }

// this is the biggie
void hashMap_Set( HashMap* hashMap, const char* key, int value )
{
	assert( hashMap != NULL );

	// find the ideal position
	// from there scan forward until we find an empty spot
	// if the spot matches (the key already exists) then set the new value
	// otherwise if the spot is empty insert the key and values, also set the probe distance
	// else if the current distance is greater than the probe distance rehash in a larger hashmap, start the Set from the beginning
	// else if the current distance is greater then the distance for the entry at this spot, swap all the values and continue

	// copy the key
	size_t len = SDL_strlen( key ) + 1;
	char* newKey = mem_Allocate( sizeof( char ) * len );
	SDL_strlcpy( newKey, key, len );
	
	uint32_t idx = hash( hashMap, newKey );
	uint8_t currLength = 0;
	bool increaseLength = true;

	while( ( hashMap->keys[idx].probeLength != UINT8_MAX ) &&
		   !matches( hashMap, newKey, idx, currLength ) ) {

		uint32_t nextIdx = NEXT_IDX( hashMap, idx );

		if( currLength > hashMap->probeLimit ) {
			// past the probe limit, this is a failed insert, rebuild and reset insertion stuff
			resize( hashMap, chooseCapacity( hashMap->capacity ) );

			nextIdx = hash( hashMap, newKey );
			currLength = 0;
			increaseLength = false;
		} else if( currLength > hashMap->keys[idx].probeLength ) {
			// current index is greater, take from the rich and give to the poor
			SWAP( hashMap->keys[idx].key, newKey, char* );
			SWAP( hashMap->keys[idx].probeLength, currLength, uint8_t );
			SWAP( hashMap->values[idx], value, int );
		}

		if( increaseLength ) ++currLength;
		idx = nextIdx;
		increaseLength = true;
	}

	// got a good spot, insert
	hashMap->keys[idx].key = newKey;
	hashMap->keys[idx].probeLength = currLength;
	hashMap->values[idx] = value;
}

// returns whether the find was a success
bool hashMap_Find( HashMap* hashMap, const char* key, int* outValue )
{
	assert( hashMap != NULL );

	uint32_t idx;
	if( !findIndex( hashMap, key, &idx ) ) {
		return false;
	}

	(*outValue) = hashMap->values[idx];
	return true;
}

bool hashMap_Exists( HashMap* hashMap, const char* key )
{
	assert( hashMap != NULL );
	uint32_t idx;
	return findIndex( hashMap, key, &idx );
}

static void removeAtIdx( HashMap* hashMap, uint32_t idx )
{
	// mark idx as unused
	hashMap->keys[idx].probeLength = UINT8_MAX;
	mem_Release( hashMap->keys[idx].key );
	hashMap->keys[idx].key = NULL;
	uint32_t prevIdx = idx;
	idx = NEXT_IDX( hashMap, idx );

	// swap with the next element as long as the next element is in use, decrement the distance by one as well
	while( ( hashMap->keys[idx].probeLength != UINT8_MAX ) && ( hashMap->keys[idx].probeLength != 0 ) ) {
		hashMap->keys[prevIdx].probeLength = hashMap->keys[idx].probeLength - 1;
		hashMap->keys[prevIdx].key = hashMap->keys[idx].key;
		hashMap->values[prevIdx] = hashMap->values[idx];

		hashMap->keys[idx].probeLength = UINT8_MAX;

		prevIdx = idx;
		idx = NEXT_IDX( hashMap, idx );
	}
}

void hashMap_Remove( HashMap* hashMap, const char* key )
{
	assert( hashMap != NULL );

	// first see if what wants to be removed exists
	uint32_t idx;
	if( !findIndex( hashMap, key, &idx ) ) {
		return;
	}

	removeAtIdx( hashMap, idx );
}

void hashMap_RemoveFirstByValue( HashMap* hashMap, int value )
{
	assert( hashMap != NULL );

	// just do this the naive way until we run into performance issues
	for( size_t i = 0; i < hashMap->capacity; ++i ) {
		if( hashMap->keys[i].probeLength != UINT8_MAX ) {
			if( hashMap->values[i] == value ) {
				removeAtIdx( hashMap, (uint32_t)i );
				return;
			}
		}
	}
}

void hashMap_Clear( HashMap* hashMap )
{
	assert( hashMap != NULL );

	// clear out keys
	for( size_t i = 0; i < hashMap->capacity; ++i ) {
		mem_Release( hashMap->keys[i].key );
	}

	mem_Release( hashMap->keys );
	mem_Release( hashMap->values );

	hashMap->keys = NULL;
	hashMap->values = NULL;

	hashMap->capacity = 0;
	hashMap->probeLimit = 0;
}

void hashMap_Report( HashMap* hashMap, size_t* capacity )
{
	assert( hashMap != NULL );

	(*capacity) = hashMap->capacity;
}

void hashMap_Test( void )
{
	HashMap testMap;

	hashMap_Init( &testMap, 1, NULL );


	llog( LOG_DEBUG, "Testing basic adding" );
	hashMap_Set( &testMap, "testing", 23 );
	int testVal;
	// adding a value
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - Added value exists." );
		if( testVal == 23 ) {
			llog( LOG_DEBUG, " - Correct added value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDED VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}

	// adding another value
	hashMap_Set( &testMap, "another test", 100 );
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - Initial added value exists." );
		if( testVal == 23 ) {
			llog( LOG_DEBUG, " - Correct added value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDED VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - INITIAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "another test", &testVal ) ) {
		llog( LOG_DEBUG, " - Additional added value exists." );
		if( testVal == 100 ) {
			llog( LOG_DEBUG, " - Correct additional value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDITIONAL VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - ADDITIONAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}


	llog( LOG_DEBUG, "Testing modifying" );
	hashMap_Set( &testMap, "testing", 99 );
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - Initial added value exists." );
		if( testVal == 99 ) {
			llog( LOG_DEBUG, " - Correct added value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDED VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - INITIAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "another test", &testVal ) ) {
		llog( LOG_DEBUG, " - Additional added value exists." );
		if( testVal == 100 ) {
			llog( LOG_DEBUG, " - Correct additional value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDITIONAL VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - ADDITIONAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}


	hashMap_Set( &testMap, "another test", 9001 );
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - Initial added value exists." );
		if( testVal == 99 ) {
			llog( LOG_DEBUG, " - Correct added value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDED VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - INITIAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "another test", &testVal ) ) {
		llog( LOG_DEBUG, " - Additional added value exists." );
		if( testVal == 9001 ) {
			llog( LOG_DEBUG, " - Correct additional value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDITIONAL VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - ADDITIONAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}


	llog( LOG_DEBUG, "Testing removing" );
	hashMap_Remove( &testMap, "testing" );
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - REMOVED VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Initial added value no longer exists exists." );
	}

	if( hashMap_Find( &testMap, "another test", &testVal ) ) {
		llog( LOG_DEBUG, " - Additional added value exists." );
		if( testVal == 100 ) {
			llog( LOG_DEBUG, " - Correct additional value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDITIONAL VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - INITIAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}

	hashMap_Remove( &testMap, "MISSING" );
	if( hashMap_Find( &testMap, "testing", &testVal ) ) {
		llog( LOG_DEBUG, " - REMOVED VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Initial added value no longer exists exists." );
	}

	if( hashMap_Find( &testMap, "another test", &testVal ) ) {
		llog( LOG_DEBUG, " - Additional added value exists." );
		if( testVal == 100 ) {
			llog( LOG_DEBUG, " - Correct additional value" );
		} else {
			llog( LOG_DEBUG, " - INCORRECT ADDITIONAL VALUE" );
		}
	} else {
		llog( LOG_DEBUG, " - INITIAL ADDED VALUE IS MISSING!" );
	}

	if( hashMap_Find( &testMap, "INVALID", &testVal ) ) {
		llog( LOG_DEBUG, " - MISSING VALUE EXISTS!" );
	} else {
		llog( LOG_DEBUG, " - Missing value does not exist." );
	}

	hashMap_Clear( &testMap );



	typedef struct {
		const char* key;
		int value;
	} HashMapTest;

	HashMapTest testValues[] = {
		{ "test 00", 0 },
		{ "test 01", 1 },
		{ "test 02", 2 },
		{ "test 03", 3 },
		{ "test 04", 4 },
		{ "test 05", 5 },
		{ "test 06", 6 },
		{ "test 07", 7 },
		{ "test 08", 8 },
		{ "test 09", 9 },
		{ "test 10", 10 },
		{ "test 11", 11 },
		{ "test 12", 12 },
		{ "test 13", 13 },
		{ "test 14", 14 },
		{ "test 15", 15 },
		{ "test 16", 16 },
		{ "test 17", 17 },
		{ "test 18", 18 },
		{ "test 19", 19 },
		{ "test 20", 20 },
		{ "test 21", 21 },
		{ "test 22", 22 },
		{ "test 23", 23 },
		{ "test 24", 24 },
		{ "test 25", 25 },
		{ "test 26", 26 },
		{ "test 27", 27 },
		{ "test 28", 28 },
		{ "test 29", 29 },
		{ "test 30", 30 },
		{ "test 31", 31 },
		{ "test 32", 32 },
		{ "test 33", 33 },
		{ "test 34", 34 },
		{ "test 35", 35 },
		{ "test 36", 36 },
		{ "test 37", 37 },
		{ "test 38", 38 },
		{ "test 39", 39 },
		{ "test 40", 40 },
		{ "test 41", 41 },
		{ "test 42", 42 },
		{ "test 43", 43 },
		{ "test 44", 44 },
		{ "test 45", 45 },
		{ "test 46", 46 },
		{ "test 47", 47 },
		{ "test 48", 48 },
		{ "test 49", 49 },
		{ "test 50", 50 },
		{ "test 51", 51 },
		{ "test 52", 52 },
		{ "test 53", 53 },
		{ "test 54", 54 },
		{ "test 55", 55 },
		{ "test 56", 56 },
		{ "test 57", 57 },
		{ "test 58", 58 },
		{ "test 59", 59 },
	};

	llog( LOG_DEBUG, "Testing mass add" );
	hashMap_Init( &testMap, 1, NULL );

	size_t arraySize = ARRAY_SIZE( testValues );
	for( size_t i = 0; i < arraySize; ++i ) {
		hashMap_Set( &testMap, testValues[i].key, testValues[i].value );

		// test to see if all previous key-value pairs still exist
		for( size_t a = 0; a < i; ++a ) {
			if( hashMap_Find( &testMap, testValues[a].key, &testVal ) ) {
				if( testVal != testValues[a].value ) {
					llog( LOG_DEBUG, " - AFTER ADDING VALUE FOR TEST PAIR %u, TESTING VALUE OF PAIR %u FAILED", i, a );
				}
			} else {
				llog( LOG_DEBUG, " - AFTER ADDING VALUE FOR TEST PAIR %u, KEY OF PAIR %u NO LONGER EXISTS", i, a );
			}
		}
	}

	llog( LOG_DEBUG, "Mass add test done" );

	hashMap_Clear( &testMap );
}