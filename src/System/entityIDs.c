#include "entityIDs.h"

#include <string.h>
#include <assert.h>

// TODO: may want to change this to use 8 bits for the generation, and 24 bits for the index
const EntityID INVALID_ENTITY = 0;

#define IS_IN_USE 0x1

typedef struct {
	uint16_t idx;
	uint16_t gen;
} EntityIDParts;

typedef union {
	uint32_t id;
	EntityIDParts idParts;
} EntityIDUnion;

#define createID( index, generation ) ( (uint32_t)index ) | ( ( (uint32_t)generation ) << 16 )

static void clear( EntityIDSet* idSet )
{
	memset( idSet->entities, 0, sizeof( idSet->entities[0] ) * MAX_ENTITIES );
	idSet->currMax = 0;
}

void eids_Clear( EntityIDSet* idSet )
{
	clear( idSet );
	//clear( ( idSet == NULL ) ? &defaultIDSet : idSet );
}

static EntityID create( EntityIDSet* idSet )
{
	assert( idSet != NULL );
	// attempt to find an unused entity, 0 is always an invalid entity
	uint16_t idx = 0;
	while( ( idx < MAX_ENTITIES ) && ( ( idSet->entities[idx].flags & IS_IN_USE ) != 0 ) ) {
		++idx;
	}

	if( idx >= MAX_ENTITIES ) {
		return 0;
	}

	// found a valid spot, mark it as in use, advance the generation, and generate the id
	idSet->entities[idx].flags |= IS_IN_USE;

	if( idSet->entities[idx].generation == UINT16_MAX ) {
		idSet->entities[idx].generation = 1;
	} else {
		++( idSet->entities[idx].generation );
	}

	if( idx > idSet->currMax ) {
		idSet->currMax = idx;
	}

	return createID( idx, idSet->entities[idx].generation );
}

EntityID eids_Create( EntityIDSet* idSet )
{
	return create( idSet );
	//return create( ( idSet == NULL ) ? &defaultIDSet : idSet );
}

static void destroy( EntityIDSet* idSet, EntityID id )
{
	assert( idSet != NULL );
	if( id == 0 ) {
		return;
	}

	EntityIDUnion eid;
	eid.id = id;

	idSet->entities[eid.idParts.idx].flags &= ~IS_IN_USE;

	// handle moving the current maximum down
	while( !( idSet->entities[idSet->currMax].flags & IS_IN_USE ) && idSet->currMax > 0 ) {
		--( idSet->currMax );
	}
}

void eids_Destroy( EntityIDSet* idSet, EntityID id )
{
	destroy( idSet, id );
	//destroy( ( idSet == NULL ) ? &defaultIDSet : idSet, id );
}

static bool isValid( EntityIDSet* idSet, EntityID id )
{
	assert( idSet != NULL );
	if( id == 0 ) {
		return false;
	}

	EntityIDUnion eid;
	eid.id = id;

	if( ( idSet->entities[eid.idParts.idx].flags & IS_IN_USE ) && ( idSet->entities[eid.idParts.idx].generation == eid.idParts.gen ) ) {
		return true;
	}

	return false;
}

bool eids_IsValid( EntityIDSet* idSet, EntityID id )
{
	return isValid( idSet, id );
	//return isValid( ( idSet == NULL ) ? &defaultIDSet : idSet, id );
}

uint32_t eids_GetIdx( EntityID id )
{
	return (uint32_t)( id & 0xFFFF );
}

static uint32_t idFromIdx( EntityIDSet* idSet, uint32_t idx )
{
	assert( idSet != NULL );
	EntityIDUnion eid;
	eid.idParts.idx = (uint16_t)idx;
	eid.idParts.gen = idSet->entities[idx].generation;
	return eid.id;
}

uint32_t eids_IDFromIdx( EntityIDSet* idSet, uint32_t idx )
{
	return idFromIdx( idSet, idx );
	//return idFromIdx( ( idSet == NULL ) ? &defaultIDSet : idSet, idx );
}

// *** for iterating through entity ids, returns 0 if there is no next entity
static EntityID getFirstID( EntityIDSet* idSet )
{
	assert( idSet != NULL );
	for( uint16_t i = 0; i < MAX_ENTITIES; ++i ) {
		if( idSet->entities[i].flags & IS_IN_USE ) {
			return createID( i, idSet->entities[i].generation );
		}
	}

	return 0;
}

EntityID eids_GetFirstID( EntityIDSet* idSet )
{
	return getFirstID( idSet );
	//return getFirstID( ( idSet == NULL ) ? &defaultIDSet : idSet );
}

static EntityID getNextID( EntityIDSet* idSet, uint32_t currID )
{
	assert( idSet != NULL );
	for( uint16_t idx = ( (uint16_t)( currID & 0xFFFF ) ) + 1; idx <= idSet->currMax; ++idx ) {
		if( idSet->entities[idx].flags & IS_IN_USE ) {
			uint32_t id = createID( idx, idSet->entities[idx].generation );
			return id;
		}
	}

	return 0;
}

EntityID eids_GetNextID( EntityIDSet* idSet, uint32_t currID )
{
	return getNextID( idSet, currID );
	//return getNextID( ( idSet == NULL ) ? &defaultIDSet : idSet, currID );
}