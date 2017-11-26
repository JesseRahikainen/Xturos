#ifndef ENTITIES_H
#define ENTITIES_H

#include <stdbool.h>
#include <stdint.h>

// should always be <= UINT16_MAX
#define MAX_ENTITIES UINT16_MAX

typedef uint32_t EntityID;

typedef struct {
	uint16_t generation;
	uint16_t flags;
} EntityStorage;

typedef struct {
	EntityStorage entities[MAX_ENTITIES];
	uint16_t currMax;
} EntityIDSet;

extern const EntityID INVALID_ENTITY;

void eids_Clear( EntityIDSet* idSet );
EntityID eids_Create( EntityIDSet* idSet );
void eids_Destroy( EntityIDSet* idSet, EntityID id );
bool eids_IsValid( EntityIDSet* idSet, EntityID id );

// *** these are for some specific component layouts
uint32_t eids_GetIdx( EntityID id );
EntityID eids_IDFromIdx( EntityIDSet* idSet, uint32_t idx );

// *** for iterating through entity ids, returns 0 if there is no next entity
EntityID eids_GetFirstID( EntityIDSet* idSet );
EntityID eids_GetNextID( EntityIDSet* idSet, EntityID currID );

#endif