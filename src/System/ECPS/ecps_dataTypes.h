#ifndef ECPS_DATA_TYPES
#define ECPS_DATA_TYPES

#include <stdint.h>
#include <stdbool.h>

#include "../../Utils/idSet.h"
#include "ecps_values.h"

typedef uint32_t ComponentID;
#define INVALID_COMPONENT_ID UINT32_MAX

typedef int (*VerifyComponent)( EntityID entityID );

typedef struct {
	char name[32];
	size_t size;

	// this was added to make sure the values of a component are correct (e.g. the character doesn't move off the screen)
	//  more of a debugging tool than something you should use in a game, think of it like an assert( )
	VerifyComponent verify; 
} ComponentType;

typedef struct {
	ComponentType* sbTypes;
} ComponentTypeCollection;

typedef struct {
	int32_t offset;
} PackageStructureEntry;

typedef struct {
	PackageStructureEntry entries[MAX_NUM_COMPONENT_TYPES];
} PackageStructure;

typedef struct {
	size_t entitySize;
	PackageStructure structure;
	uint8_t* sbData;
} PackagedComponentArray;

// used for accessing an entity directly
typedef struct {
	int32_t packedArrayIdx;		// either the array index, or -1 if the entity doesn't exist
	size_t positionOffset;		// if the packedArrayIdx is >= 0 then this is the offset into the byte array where it starts
} EntityDirectoryEntry;

typedef struct {
	uint32_t bits[FLAGS_ARRAY_SIZE];
} ComponentBitFlags;

typedef struct {
	EntityDirectoryEntry* sbEntityDirectory;	// holds the offset into the data where the entity starts

	// the sizes of these two should be the same
	ComponentBitFlags* sbBitFlags;				// what components the array contains
	PackagedComponentArray* sbComponentArrays;	// structure information and the entity data
} ComponentData;

typedef struct {
	ComponentData componentData;
	ComponentTypeCollection componentTypes;
	bool isRunning;
	uint32_t id;
	IDSet idSet;
	uint8_t* sbCommandBuffer;
	bool isRunningProcess;
} ECPS;

typedef struct {
	EntityID id;
	void* data;
	const PackageStructure* structure;
} Entity;

typedef void (*PreProcFunc)( ECPS* ecps );
typedef void (*ProcFunc)( ECPS* ecps, const Entity* entity );
typedef void (*PostProcFunc)( ECPS* ecps );

typedef struct {
	uint32_t ecpsID;
	PreProcFunc preProc;
	ProcFunc proc;
	PostProcFunc postProc;

	ComponentBitFlags bitFlags;

	char name[32];
} Process;

#endif