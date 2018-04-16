#include "idSet.h"

#include <assert.h>
#include "stretchyBuffer.h"

// TODO: Merge this into entityIDs.c, the only difference is this one can handle a variable amount of ids and the other can't
//  lets do this the other way, merge entityIDs into idSet, just need to figure out how it's different and adjust for any speed differences
//  or could just leave them separate for strategic, untested optimization reasons (i.e. laziness)

#define IS_IN_USE 0x1

typedef struct {
	uint16_t idx;
	uint16_t gen;
} IDParts;

typedef union {
	EntityID id;
	IDParts idParts;
} IDUnion;

#define createID( index, generation ) ( (uint32_t)index ) | ( ( (uint32_t)generation ) << 16 )

/*
Initializes an IDSet, the maximum number of ids allowed is set in maxSize.
Max size can never be larger than 2^16.
 Returns 0 if it was a success, a negative number otherwise.
*/
int idSet_Init( IDSet* set, size_t maxSize )
{
	assert( set != NULL );
	assert( maxSize <= (size_t)UINT16_MAX );

	sb_Add( set->sbIDData, maxSize );
	idSet_Clear( set );

	set->currMaxCount = 0;

	return 0;
}

/*
Releases all the memory in use by an idSet.
*/
void idSet_Destroy( IDSet* set )
{
	assert( set != NULL );
	sb_Release( set->sbIDData );
	set->sbIDData = NULL;
}

/*
Claims an id and returns it, returns a value of 0 if there were none available.
*/
EntityID idSet_ClaimID( IDSet* set )
{
	assert( set != NULL );
	// attempt to find an unused id, 0 is always an invalid id
	uint16_t idx = 0;
	size_t count = sb_Count( set->sbIDData );

	while( ( idx < count ) && ( ( set->sbIDData[idx].flags & IS_IN_USE ) != 0 ) ) {
		++idx;
	}

	if( idx >= count ) {
		return 0;
	}

	// found valid id, mark it as in use, advance the generation, and generate the id
	set->sbIDData[idx].flags |= IS_IN_USE;

	if( set->sbIDData[idx].generation == UINT16_MAX ) {
		set->sbIDData[idx].generation = 1;
	} else {
		++( set->sbIDData[idx].generation );
	}

	if( idx > set->currMaxCount ) {
		set->currMaxCount = idx;
	}

	return createID( idx, set->sbIDData[idx].generation );
}

/*
Releases an id from use, allowing it to be used by something else.
*/
void idSet_ReleaseID( IDSet* set, EntityID id )
{
	assert( set != NULL );

	if( id == 0 ) {
		return;
	}

	IDUnion idu;
	idu.id = id;

	set->sbIDData[idu.idParts.idx].flags &= ~IS_IN_USE;

	while( !( set->sbIDData[set->currMaxCount].flags & IS_IN_USE ) && ( set->currMaxCount > 0 ) ) {
		--( set->currMaxCount );
	}
}

/*
Sets a new maximum number of ids, will not shrink it if the new is less than the current.
*/
void idSet_IncreaseMaximum( IDSet* set, size_t newMax )
{
	assert( set != NULL );

	if( newMax <= sb_Count( set->sbIDData ) ) {
		return;
	}

	size_t growAmt = newMax - sb_Count( set->sbIDData );
	IDStorage* startNew = sb_Add( set->sbIDData, growAmt );
	memset( startNew, 0, sizeof( startNew[0] ) * growAmt );
}

/*
Clears all the ids.
*/
void idSet_Clear( IDSet* set )
{
	assert( set != NULL );
	memset( set->sbIDData, 0, sizeof( set->sbIDData[0] ) * sb_Count( set->sbIDData ) );
}

/*
Returns whether the id passed in is currently claimed or not.
*/
bool idSet_IsIDValid( IDSet* set, EntityID id )
{
	assert( set != NULL );

	if( id == 0 ) {
		return false;
	}

	IDUnion idu;
	idu.id = id;
	if( ( set->sbIDData[idu.idParts.idx].flags & IS_IN_USE ) && ( set->sbIDData[idu.idParts.idx].generation == idu.idParts.gen ) ) {
		return true;
	}

	return false;
}

/*
Returns an index associated with this id. Does no checking to see if it's valid.
*/
uint16_t idSet_GetIndex( EntityID id )
{
	IDUnion idu;
	idu.id = id;
	return idu.idParts.idx;
}

/*
Generates an id given an index. Does no checking to see if it's valid.
*/
EntityID idSet_GetIDFromIndex( IDSet* set, uint16_t index )
{
	if( index >= sb_Count( set->sbIDData ) ) {
		return 0;
	}

	return createID( index, set->sbIDData[index].generation );
}

/*
Returns the first valid id, returns 0 if there is none.
*/
EntityID idSet_GetFirstValidID( IDSet* set )
{
	assert( set != NULL );

	uint16_t cnt = (uint16_t)sb_Count( set->sbIDData );
	for( uint16_t i = 0; i < cnt; ++i ) {
		if( set->sbIDData[i].flags & IS_IN_USE ) {
			return createID( i, set->sbIDData[i].generation );
		}
	}

	return 0;
}

/*
Returns the first valid id after the passed in id, returns 0 if there is none.
*/
EntityID idSet_GetNextValidID( IDSet* set, EntityID id )
{
	for( uint16_t idx = idSet_GetIndex( id ) + 1; idx <= set->currMaxCount; ++idx ) {
		if( set->sbIDData[idx].flags & IS_IN_USE ) {
			return createID( idx, set->sbIDData[idx].generation );
		}
	}
	
	return 0;
}