/*
based on this: https://probablydance.com/2017/02/26/i-wrote-the-fastest-hashtable/
 - Open addressing
 - Linear probing
 - Robin Hood hashing (When you probe for an insertion, if the probe length (distance from the ideal spot) for the existing element is less than the probe
	  length for the element being inserted, swap the two elements and keep going.) This lets us add a test when retrieving, such that if the distance from
    the ideal spot is greater than the distance that the current slot is from it's ideal, we can assume the item doesn't exist.
    When deleting an element we'll NEED to find if there was something that could fill it's spot, this is quite easy, go forward while there are still
    elements, if the element is not in it's ideal spot move it back one (and decrement it's probe distance accordingly).
 - Power of two number of slots (they recommend primes, but we shouldn't run into the issues presented (hopefully))
 - Upper limit on the probe count (recommends log2(n) where n is the number of slots in the table), when we try to insert and
    hit the upper limit probe count we resize the table to be larger, generally this was at about 2/3 full. Can add a test for retrieving, if the distance
    from the ideal spot is greater than the max probe count we know the item cannot exist (this will end up being equivilant to the test from the Robin
    Hood hashing retrieval test).
NOTE: We're storing the string pointer passed in, so we're assuming it will not change. Can modify it later to make a copy of the string instead.
*/

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef uint32_t (*HashFunc)( const char* );

typedef struct {
	const char* key;
	uint8_t probeLength; // how far we are from our ideal spot
	// using the system we'll have setup, the probe length should never be greater than log2(n), so if we set it to UINT8_MAX we can use that
	//  as the empty spot flag
} HashMapKey;

typedef struct {
	size_t capacity; // number of spots available
	HashMapKey* keys;
	int* values;
	uint32_t probeLimit;
	HashFunc hashFunc;
} HashMap;

// if hashFunc == NULL will use a default function
void hashMap_Init( HashMap* hashMap, uint32_t estimatedSize, HashFunc hashFunc );

// if the value already exists it will replace it, otherwise it will add it
void hashMap_Set( HashMap* hashMap, const char* key, int value );

// Returns whether the key was found, if it was the value is place in outValue
bool hashMap_Find( HashMap* hashMap, const char* key, int* outValue );

// Returns whether the key exists
bool hashMap_Exists( HashMap* hashMap, const char* key );

// Removes a key and value pair
void hashMap_Remove( HashMap* hashMap, const char* key );

// Clears all the stored keys and values
void hashMap_Clear( HashMap* hashMap );

// Used for debugging
void hashMap_Report( HashMap* hashMap, size_t* capacity );

#endif /* inclusion guard */