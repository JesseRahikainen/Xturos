#ifndef ECPS_DATA_TYPES
#define ECPS_DATA_TYPES

#include <stdint.h>
#include <stdbool.h>

#include "Utils/idSet.h"
#include "ecps_values.h"
#include "Others/cmp.h"

// doesn't include the terminating null
#define MAX_COMPONENT_NAME_SIZE 32

typedef uint32_t ComponentID;
#define INVALID_COMPONENT_ID UINT32_MAX

// serialization specific data structures
typedef struct {
	uint32_t internalID; // id used internally in the serialized info

	// used to map the serialized components to current engine components
	char externalID[MAX_COMPONENT_NAME_SIZE + 1];
	uint32_t externalVersion;

	ComponentID ecpsComponentID; // this is used internally while serializing and deserializing
	bool used; // whether the component is used during the serialization
} SerializedComponentInfo;

typedef struct {
	uint32_t internalCompID; // matches internalID in SerializedComponentInfo

	uint32_t dataSize; // the data for the serialize component
	uint8_t* data;
} SerializedComponent;

typedef struct {
	uint32_t internalID; // id used internally in the serialized info
	SerializedComponent* sbCompData; // all the components in the entity

	// when serializing the ecpsEntityID is set from the ECPS and the internalID is created
	// when deserializing the internalID is loaded in and the ecpsEntityID is created and stored here
	EntityID ecpsEntityID;
} SerializedEntityInfo;

typedef struct {
	SerializedComponentInfo* sbCompInfos;
	SerializedEntityInfo* sbEntityInfos;
} SerializedECPS;

// component callbacks
typedef int ( *VerifyComponent )( EntityID entityID );
typedef void ( *CleanUpComponent )( void* data );
typedef bool ( *SerializeComponent )( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo );
typedef bool ( *DeserializeComponent )( cmp_ctx_t* cmp, void* outData, SerializedEntityInfo* sbEntityInfo );

typedef struct {
	char name[MAX_COMPONENT_NAME_SIZE + 1];
	uint32_t version;
	size_t size;
	size_t align; // desired aligment of the structure

	// this was added to make sure the values of a component are correct (e.g. the character doesn't move off the screen)
	//  more of a debugging tool than something you should use in a game, think of it like an assert( )
	VerifyComponent verify;

	// used when there is clean up that needs to be done after a component is removed or
	//  an entity containing it is destroyed
	CleanUpComponent cleanUp;

	// serialization functions, used to save out the components, if you don't need to worry about serializing or deserializing stuff you can ignore it
	SerializeComponent serialize;
	DeserializeComponent deserialize;
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
	size_t firstAlign;
	PackageStructure structure;
	uint8_t* sbData;
	uint8_t* dataStart; // first spot in sbData that matches firstAlign
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