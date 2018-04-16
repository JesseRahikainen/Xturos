#ifndef ID_SET_H
#define ID_SET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t EntityID;

#define INVALID_ENTITY_ID 0

/*
Used to store a set of reference ids.
*/

typedef struct {
	uint16_t flags;
	uint16_t generation;
} IDStorage;

typedef struct {
	IDStorage* sbIDData;
	uint16_t currMaxCount;
} IDSet;

/*
Initializes an IDSet, the maximum number of ids allowed is set in maxSize.
Max size can never be larger than 2^16.
 Returns 0 if it was a success, a negative number otherwise.
*/
int idSet_Init( IDSet* set, size_t maxSize );

/*
Releases all the memory in use by an idSet.
*/
void idSet_Destroy( IDSet* set );

/*
Claims an id and returns it, returns a value of 0 if there were none available.
*/
EntityID idSet_ClaimID( IDSet* set );

/*
Releases an id from use, allowing it to be used by something else.
*/
void idSet_ReleaseID( IDSet* set, EntityID id );

/*
Sets a new maximum number of ids, will not shrink it if the new is less than the current.
*/
void idSet_IncreaseMaximum( IDSet* set, size_t newMax );

/*
Clears all the ids.
*/
void idSet_Clear( IDSet* set );

/*
Returns whether the id passed in is currently claimed or not.
*/
bool idSet_IsIDValid( IDSet* set, EntityID id );

/*
Returns an index associated with this id. Does no checking to see if it's valid.
*/
uint16_t idSet_GetIndex( EntityID id );

/*
Generates an id given an index. Does no checking to see if it's valid.
 Returns 0 if the index is out of range
*/
EntityID idSet_GetIDFromIndex( IDSet* set, uint16_t index );

/*
Returns the first valid id, returns 0 if there is none.
*/
EntityID idSet_GetFirstValidID( IDSet* set );

/*
Returns the first valid id after the passed in id, returns 0 if there is none.
*/
EntityID idSet_GetNextValidID( IDSet* set, EntityID id );

#endif