#include "hashMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "../Math/mathUtil.h"
#include "../System/memory.h"

uint32_t hashFunc_DJB2( const char* str )
{
	uint32_t hash = 5381;
	uint32_t c;

	while( c = *str++ ) {
		hash = ( ( hash << 5 ) + hash ) + c;
	}

	return hash;
}

bool isPowerOfTwoU32( uint32_t val )
{
	return ( ( val == 1 ) || ( ( val & ( val - 1 ) ) == 0 ) );
}

// we're assuming denom is a power of two
uint32_t powerTwoModulus( uint32_t num, uint32_t denom )
{
	assert( isPowerOfTwoU32( denom ) );
	return ( num & ( denom - 1 ) );
}

uint32_t chooseCapacity( uint32_t baseSize )
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
uint32_t hash( HashMap* hashMap, const char* key )
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

	memset( hashMap->keys, UINT8_MAX, sizeof( HashMapKey ) * hashMap->capacity );

	hashMap->probeLimit = (size_t)log2( (double)hashMap->capacity );
}

void resize( HashMap* hashMap, uint32_t newSize )
{
	// allocate new list and move data
	HashMap newHashMap;
	hashMap_Init( &newHashMap, newSize, hashMap->hashFunc );

	// insert elements from old hashmap
	for( size_t i = 0; i < hashMap->capacity; ++i ) {
		if( hashMap->keys[i].probeLength != UINT8_MAX ) {
			hashMap_Set( &newHashMap, hashMap->keys[i].key, hashMap->values[i] );
		}
	}

	// release old data
	mem_Release( hashMap->keys );
	mem_Release( hashMap->values );

	// copy over new data
	(*hashMap) = newHashMap;
}

bool matches( HashMap* hashMap, const char* key, int idx, uint8_t currLength )
{
	return ( ( hashMap->keys[idx].probeLength == currLength ) && ( strcmp( key, hashMap->keys[idx].key ) == 0 ) );
}

// returns whether the key was found, if it was outIdx will contain the index
bool findIndex( HashMap* hashMap, const char* key, uint32_t* outIdx )
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

	uint32_t idx = hash( hashMap, key );
	uint8_t currLength = 0;
	bool increaseLength = true;
	while( ( hashMap->keys[idx].probeLength != UINT8_MAX ) &&
		   !matches( hashMap, key, idx, currLength ) ) {

		uint32_t nextIdx = NEXT_IDX( hashMap, idx );

		if( currLength > hashMap->probeLimit ) {
			// past the probe limit, this is a failed insert, rebuild and reset insertion stuff
			resize( hashMap, chooseCapacity( hashMap->capacity ) );

			nextIdx = hash( hashMap, key );
			currLength = 0;
			increaseLength = false;
		} else if( currLength > hashMap->keys[idx].probeLength ) {
			// current index is greater, take from the rich and give to the poor
			SWAP( hashMap->keys[idx].key, key, char* );
			SWAP( hashMap->keys[idx].probeLength, currLength, uint8_t );
			SWAP( hashMap->values[idx], value, int );
		}

		if( increaseLength ) ++currLength;
		idx = nextIdx;
		increaseLength = true;
	}

	// got a good spot, insert
	hashMap->keys[idx].key = key;
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

void hashMap_Remove( HashMap* hashMap, const char* key )
{
	assert( hashMap != NULL );

	// first see if what wants to be removed exists
	uint32_t idx;
	if( !findIndex( hashMap, key, &idx ) ) {
		return;
	}

	// mark idx as unused
	hashMap->keys[idx].probeLength = UINT8_MAX;
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

void hashMap_Clear( HashMap* hashMap )
{
	assert( hashMap != NULL );

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