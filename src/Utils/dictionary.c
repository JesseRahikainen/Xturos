#include "dictionary.h"

#include <stddef.h>
#include <assert.h>

#include "stretchyBuffer.h"

// from: http://www.cse.yorku.ca/~oz/hash.html
static size_t hashFunc_djb2( const char* str )
{
	size_t val = 5381;
	size_t c;

	while( c = *str++ ) {
		val = ( ( val << 5 ) + val ) + c;
	}
	return val;
}

static bool getKeyIndex( Dictionary* dict, const char* key, size_t* outIdx )
{
	return 0;
}

static void resize( Dictionary* dict, size_t newSize )
{
	// create new arrays for all the data and rehash everything
	//Dictionary newDict;
}

 // Initializes everything for the dictionary, returns a negative number if it fails.
int dict_Init( Dictionary* dict )
{
	assert( dict != NULL );

	dict->sbKeys = NULL;
	dict->sbValues = NULL;

	return 0;
}

// Cleans up everything for the dictionary
void dict_Destroy( Dictionary* dict )
{
	assert( dict != NULL );
}

// Sets the value for the key, if it doesn't currently exist it will create a new tuple
void dict_Set( Dictionary* dict, const char* key, int newValue )
{
	assert( dict != NULL );
	assert( key != NULL );

	size_t idx;
	if( getKeyIndex( dict, key, &idx ) ) {
		// set
		dict->sbValues[idx] = newValue;
	} else {
		// add
		idx = hashFunc_djb2( key ) % sb_Count( dict->sbValues );
	}
}

// Removes the key and it's associated value from the dictionary
void dict_Remove( Dictionary* dict, const char* key )
{
	assert( dict != NULL );
	assert( key != NULL );
}

// Returns if the key exists in the dictionary.
bool dict_Exists( Dictionary* dict, const char* key )
{
	assert( dict != NULL );
	assert( key != NULL );

	return false;
}

// Get the value for a key. Puts the result into outValue and returns if
//  anything was found.
bool dict_Get( Dictionary* dict, const char* key, int* outValue )
{
	assert( dict != NULL );
	assert( outValue != NULL );

	size_t idx;
	if( getKeyIndex( dict, key, &idx ) ) {
		(*outValue) = dict->sbValues[idx];
		return true;
	}

	return false;
}

// used for testing dictionary performance, don't use in performance heavy code
size_t dict_NumEntries( Dictionary* dict )
{
	return 0;
}

size_t dict_NumCollisions( Dictionary* dict )
{
	return 0;
}