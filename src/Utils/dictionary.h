#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <stddef.h>
#include <stdbool.h>

// start out with a <string, int> Dictionary, make it generic later if we find a use for it
// will implement it with a simple hash table
// what sort of collision handling do we want?
// fix the size of the string? if we only use the local file name it shouldn't be too much of
//  an issue (as long as we don't create tons of subfolders)

typedef struct {
	char** sbKeys;
	int* sbValues;
	
	// for use when we want to make this more generic later
	//int (*keyComp)( const char* s1, const char* s2 ); // should return 0 if they're equal
	//int (*hashFunction)( const char* );
} Dictionary;

// Initializes everything for the dictionary, returns a negative number if it fails.
int dict_Init( Dictionary* dict );

// Cleans up everything for the dictionary
void dict_Destroy( Dictionary* dict );

// Sets the value for the key, if it doesn't currently exist it will create a new tuple
void dict_Set( Dictionary* dict, const char* key, int newValue );

// Removes the key and it's associated value from the dictionary
void dict_Remove( Dictionary* dict, const char* key );

// Returns if the key exists in the dictionary.
bool dict_Exists( Dictionary* dict, const char* key );

// Get the value for a key. Puts the result into outValue and returns if
//  anything was found.
bool dict_Get( Dictionary* dict, const char* key, int* outValue );

// used for testing dictionary performance, don't use in performance heavy code
size_t dict_NumEntries( Dictionary* dict );
size_t dict_NumCollisions( Dictionary* dict );

#endif /* inclusion guard */