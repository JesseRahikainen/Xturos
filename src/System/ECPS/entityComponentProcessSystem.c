#include "entityComponentProcessSystem.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "../../Utils/stretchyBuffer.h"

#include "../../Utils/idSet.h"
#include "ecps_componentTypes.h"
#include "ecps_values.h"

#include "../platformLog.h"

static const EntityDirectoryEntry EMPTY_EDE = { -1, 0 };
static const size_t ID_SET_SIZE = UINT16_MAX;

typedef enum {
	CMD_INVALID,
	CMD_CREATE_ENTITY,
	CMD_DESTROY_ENTITY,
	CMD_ADD_COMPONENT,
	CMD_REMOVE_COMPONENT
} CommandType;

// the data after this command will define the components added and the initial data for them
typedef struct {
	CommandType cmd;
	EntityID id;
	uint32_t numComps;
	//uint8_t* data;
} CreateEntityCommand;

typedef struct {
	CommandType cmd;
	EntityID id;
} DestroyEntityCommand;

// the data after this command will define the initial value for the components
typedef struct {
	CommandType cmd;
	EntityID id;
	ComponentID compID;
	//uint8_t* data;
} AddComponentCommand;

typedef struct {
	CommandType cmd;
	EntityID id;
	ComponentID compID;
} RemoveComponentCommand;

typedef union {
	CommandType cmd;
	CreateEntityCommand create;
	DestroyEntityCommand destroy;
	AddComponentCommand add;
	RemoveComponentCommand remove;
} Command;

static uint8_t* runCreateCommand( ECPS* ecps, uint8_t* commandData );
static uint8_t* runAddComponentCommand( ECPS* ecps, uint8_t* commandData );
static uint8_t* runRemoveComponentCommand( ECPS* ecps, uint8_t* commandData );
static uint8_t* runDestroyEntityCommand( ECPS* ecps, uint8_t* commandData );

//EntityIDSet idSet;

// just a simple number to track whether processes were created with the associated system or not
static uint32_t ecpsCurrID = 0;

//*************************************************************************************

static bool createProcessVA( ECPS* ecps,
	const char* name, PreProcFunc preProc, ProcFunc proc, PostProcFunc postProc,
	Process* outProcess, size_t numComponents, va_list list )
{
	outProcess->preProc = preProc;
	outProcess->proc = proc;
	outProcess->postProc = postProc;

	if( name != NULL ) {
		strncpy( outProcess->name, name, sizeof( outProcess->name ) );
	}

	// verify all the components are valid
	memset( &( outProcess->bitFlags ), 0, sizeof( ComponentBitFlags ) );
	for( size_t i = 0; i < numComponents; ++i ) {
		ComponentID compID = va_arg( list, ComponentID );
		assert( ecps_ct_IsComponentTypeValid( &( ecps->componentTypes ), compID ) );
		ecps_cbf_SetFlagOn( &( outProcess->bitFlags ), compID );
	}

	// all processes require the ID and enabled components
	ecps_cbf_SetFlagOn( &( outProcess->bitFlags ), sharedComponent_Enabled );
	ecps_cbf_SetFlagOn( &( outProcess->bitFlags ), sharedComponent_ID );

	outProcess->ecpsID = ecps->id;

	return true;
}

static void modifyEntityDirectoryEntry( ECPS* ecps, EntityID entityID, int32_t packedArrayIdx, size_t offset )
{
	size_t idx = (size_t)idSet_GetIndex( entityID );
	int cnt = sb_Count( ecps->componentData.sbEntityDirectory );

	// grow if necessary
	while( idx >= sb_Count( ecps->componentData.sbEntityDirectory ) ) {
		sb_Push( ecps->componentData.sbEntityDirectory, EMPTY_EDE );
	}

	ecps->componentData.sbEntityDirectory[idx].packedArrayIdx = packedArrayIdx;
	ecps->componentData.sbEntityDirectory[idx].positionOffset = offset;
}

static void entityCopy( ECPS* ecps,
	uint8_t* fromData, const PackageStructure* fromStructure, uint8_t* toData, const PackageStructure* toStructure )
{
	size_t componentCount = ecps_ct_ComponentTypeCount( &( ecps->componentTypes ) );
	for( size_t i = 0; i < componentCount; ++i ) {
		size_t size = ecps_ct_GetComponentTypeSize( &( ecps->componentTypes ), i );
		int32_t fromOffset = fromStructure->entries[i].offset;
		int32_t toOffset = toStructure->entries[i].offset;

		if( ( fromOffset >= 0 ) && ( toOffset >= 0 ) ) {
			// both the structures contain this component, copy over
			memcpy( &( toData[toOffset] ), &( fromData[fromOffset] ), size );
		} else if( toOffset >= 0 ) {
			// the from structure doesn't contain this component, set to zero
			memset( &( toData[toOffset] ), 0, size );
		}
	}
}

static size_t allocateDataForEntity( ECPS* ecps, int32_t packedArrayIndex )
{
	PackagedComponentArray* pca = &( ecps->componentData.sbComponentArrays[packedArrayIndex] );

	// find the first empty space
	//  scan for an entity with a 0 as the entityID (should always be the first component
	//  in every entity).
	size_t currOffset = 0;
	uint8_t* entityData = NULL;
	size_t dataSize = sb_Count( pca->sbData );
	while( ( entityData == NULL ) && ( currOffset < dataSize ) ) {
		if( ( *( (EntityID*)( &( pca->sbData[currOffset] ) ) ) ) == 0 ) {
			entityData = &( pca->sbData[currOffset] );
		} else {
			currOffset += pca->entitySize;
		}
	}

	if( entityData == NULL ) {
		currOffset = sb_Count( pca->sbData );
		sb_Add( pca->sbData, pca->entitySize );
		entityData = &( pca->sbData[currOffset] );
		memset( entityData, 0, pca->entitySize );
	}

	return currOffset;
}

static void freeUpDataFromEntity( ECPS* ecps, int32_t packedArrayIndex, size_t offset )
{
	PackagedComponentArray* pca = &( ecps->componentData.sbComponentArrays[packedArrayIndex] );
	uint8_t* entityData = &( pca->sbData[offset] );
	memset( entityData, 0, pca->entitySize );
}

static uint32_t createNewPackagedArray( ECPS* ecps,  const ComponentBitFlags* flags )
{
	PackagedComponentArray newArray;
	ComponentBitFlags newBitFlags;

	// set up the structure
	size_t currentOffset = 0;
	size_t cnt = ecps_ct_ComponentTypeCount( &( ecps->componentTypes ) );
	for( size_t i = 0; i < cnt; ++i ) {
		// all packaged arrays need the component id
		if( ( i == sharedComponent_ID ) || ecps_cbf_IsFlagOn( flags, i ) ) {
			newArray.structure.entries[i].offset = currentOffset;
			currentOffset += ecps_ct_GetComponentTypeSize( &( ecps->componentTypes ), i );
		} else {
			newArray.structure.entries[i].offset = -1;
		}
	}

	newArray.entitySize = currentOffset;

	newArray.sbData = NULL;

	// add the bit flags to the bit flags array
	memcpy( &newBitFlags, flags, sizeof( ComponentBitFlags ) );
	sb_Push( ecps->componentData.sbBitFlags, newBitFlags );
	// add the matching packaged component array
	sb_Push( ecps->componentData.sbComponentArrays, newArray );

	return sb_Count( ecps->componentData.sbComponentArrays ) - 1;
}

// finds the index for the packaged array that contains the set component bits
//  if we find the array 
static uint32_t createOrFindPackagedArray( ECPS* ecps, const ComponentBitFlags* flags )
{
	PackagedComponentArray* componentArray = NULL;
	uint32_t dataArrayIdx = 0;
	uint32_t cnt = (uint32_t)sb_Count( ecps->componentData.sbBitFlags );
	for( uint32_t i = 0; i < cnt; ++i ) {
		if( ecps_cbf_CompareExact( flags, &( ecps->componentData.sbBitFlags[i] ) ) ) {
			dataArrayIdx = i;
			componentArray = &( ecps->componentData.sbComponentArrays[i] );
			break;
		}
	}

	if( componentArray == NULL ) {
		// didn't find a matching array, attempt to create a new one that matches
		dataArrayIdx = createNewPackagedArray( ecps, flags );
	}

	return dataArrayIdx;
}

static void removeEntityFromArray( ECPS* ecps, EntityID entityID )
{
	// find entity spot
	uint32_t idx = idSet_GetIndex( entityID );
	assert( idx < sb_Count( ecps->componentData.sbEntityDirectory ) );
	int32_t packedArrayIdx = ecps->componentData.sbEntityDirectory[idx].packedArrayIdx;
	EntityDirectoryEntry* removedEDE = &( ecps->componentData.sbEntityDirectory[idx] );
	uint8_t* entityToRemoveData = &( ecps->componentData.sbComponentArrays[packedArrayIdx].sbData[removedEDE->positionOffset] );

	memset( entityToRemoveData, 0, ecps->componentData.sbComponentArrays[packedArrayIdx].entitySize );

	modifyEntityDirectoryEntry( ecps, entityID, -1, 0 );
}

//*************************************************************************************
// Public Interface
// Sets up the ecps, ready to have components, processes, and entities created
void ecps_StartInitialization( ECPS* ecps )
{
	assert( ecps != NULL );
	assert( ecpsCurrID < UINT32_MAX && "Creating too many ecps systems" );

	ecps->isRunning = false;

	ecps->sbCommandBuffer = NULL;
	ecps->isRunningProcess = true;

	ecps_ct_Init( &( ecps->componentTypes ) );
	ecps->id = ecpsCurrID;
	++ecpsCurrID;

	idSet_Init( &( ecps->idSet ), ID_SET_SIZE );

	ecps->componentData.sbBitFlags = NULL;
	ecps->componentData.sbComponentArrays = NULL;
	ecps->componentData.sbEntityDirectory = NULL;
}

// Switches states, no way to change back to the initialization state
void ecps_FinishInitialization( ECPS* ecps )
{
	ecps->isRunning = true;
	ecps->isRunningProcess = false;
}

// Clean up all the resources
void ecps_CleanUp( ECPS* ecps )
{
	assert( ecps != NULL );

	sb_Release( ecps->sbCommandBuffer );
	ecps_ct_CleanUp( &( ecps->componentTypes ) );
	idSet_Destroy( &( ecps->idSet ) );
}

// adds a component type and returns the id to reference it by
//  this can only be done before
ComponentID ecps_AddComponentType( ECPS* ecps, const char* name, size_t size, VerifyComponent verify )
{
	assert( ecps != NULL );

	// component types can only be added before we've started running
	assert( !( ecps->isRunning ) );
	assert( sb_Count( ecps->componentTypes.sbTypes ) < MAX_NUM_COMPONENT_TYPES );
	assert( sb_Count( ecps->componentTypes.sbTypes ) < INVALID_COMPONENT_ID );

	ComponentType newType;

	newType.size = size;
	newType.verify = verify;

	if( name != NULL ) {
		strncpy( newType.name, name, sizeof( newType.name ) - 1 );
		newType.name[sizeof( newType.name ) - 1] = 0;
	}

	ComponentID id = (ComponentID)sb_Count( ecps->componentTypes.sbTypes );
	sb_Push( ecps->componentTypes.sbTypes, newType );

	return id;
}

// this attempts to set up a process to be used by the passed in ecps
bool ecps_CreateProcess( ECPS* ecps,
	const char* name, PreProcFunc preProc, ProcFunc proc, PostProcFunc postProc,
	Process* outProcess, size_t numComponents, ... )
{
	assert( ecps != NULL );
	assert( outProcess != NULL );

	bool success = false;

	va_list list;
	va_start( list, numComponents );
	success = createProcessVA( ecps, name, preProc, proc, postProc, outProcess, numComponents, list );
	va_end( list );

	return success;
}

// run a process, must have been created with the associated entity-component-process system
void ecps_RunProcess( ECPS* ecps, Process* process )
{
	assert( ecps != NULL );
	assert( ecps->isRunning );

	size_t presize = sb_Count( ecps->sbCommandBuffer );

	// verify the process is part of the entity-component-process system
	assert( ( ecps->id ) == ( process->ecpsID ) );

	// then run the process, looping through all the entities
	if( process->preProc != NULL ) {
		process->preProc( ecps );
	}

	ecps->isRunningProcess = true;
	if( process->proc != NULL ) {
		// will need to iterate through all entities that have the components the process is looking for
		size_t numCompArrays = sb_Count( ecps->componentData.sbComponentArrays );
		for( size_t cai = 0; cai < numCompArrays; ++cai ) {
			PackagedComponentArray* pca = &( ecps->componentData.sbComponentArrays[cai] );
			ComponentBitFlags* cbf = &( ecps->componentData.sbBitFlags[cai] );
			if( ecps_cbf_CompareContains( &( process->bitFlags ), cbf ) ) {
				// component data array matches, iterate through entities
				size_t dataIdx = 0;
				size_t dataArraySize = sb_Count( pca->sbData );
				while( dataIdx < dataArraySize ) {
					// first should always be the entity id
					void* data = (void*)( &( pca->sbData[dataIdx] ) );
					EntityID entityID = *( (EntityID*)data );
					if( entityID != INVALID_ENTITY_ID ) {
						Entity entity;
						entity.id = entityID;
						entity.data = data;
						entity.structure = &( pca->structure );
						process->proc( ecps, &entity );
					}
					dataIdx += pca->entitySize;
				}
			}
		}
	}
	ecps->isRunningProcess = false;

	if( process->postProc != NULL ) {
		process->postProc( ecps );
	}

	if( sb_Count( ecps->sbCommandBuffer ) > 0 ) {
		// run the command buffer
		uint8_t* cmdBuffer = ecps->sbCommandBuffer;
		uint8_t* bufferEnd = &( sb_Last( ecps->sbCommandBuffer ) );

		size_t size = sb_Count( ecps->sbCommandBuffer );
		int numCmds = 0;
		while( cmdBuffer < bufferEnd ) {
			mem_Verify( );
			++numCmds;
			CommandType cmdType = *( (CommandType*)cmdBuffer );
			switch( cmdType ) {
			case CMD_ADD_COMPONENT:
				cmdBuffer = runAddComponentCommand( ecps, cmdBuffer );
				break;
			case CMD_CREATE_ENTITY:
				cmdBuffer = runCreateCommand( ecps, cmdBuffer );
				break;
			case CMD_DESTROY_ENTITY:
				cmdBuffer = runDestroyEntityCommand( ecps, cmdBuffer );
				break;
			case CMD_REMOVE_COMPONENT:
				cmdBuffer = runRemoveComponentCommand( ecps, cmdBuffer );
				break;
			default:
				assert( false && "Invalid command" );
				break;
			}
			mem_Verify( );
			assert( cmdBuffer <= ( bufferEnd + 1 ) );
		}

		sb_Clear( ecps->sbCommandBuffer );
	}
}

static void createEntityVA( ECPS* ecps, EntityID entityID, size_t numComponents, va_list va )
{
	va_list list;

	ComponentBitFlags entityBitFlags;

	va_copy( list, va ); {
		memset( &entityBitFlags, 0, sizeof( ComponentBitFlags ) );
		for( size_t i = 0; i < numComponents; ++i ) {
			ComponentID compID = va_arg( list, ComponentID );
			va_arg( list, void* );
			//assert( ecps_ct_IsComponentTypeValid( &( ecps->componentTypes ), compID ) );
			if( ecps_ct_IsComponentTypeValid( &( ecps->componentTypes ), compID ) ) {
				ecps_cbf_SetFlagOn( &entityBitFlags, compID );
			} else {
				llog( LOG_DEBUG, "Creating an entity with invalid component at varg position: %i", i );
			}
		}
	} va_end( list );

	ecps_cbf_SetFlagOn( &entityBitFlags, sharedComponent_ID );
	ecps_cbf_SetFlagOn( &entityBitFlags, sharedComponent_Enabled );

	// find or create the packaged array for this entity
	uint32_t pcaIdx = createOrFindPackagedArray( ecps, &entityBitFlags );

	// add the entity to the list
	size_t currOffset = allocateDataForEntity( ecps, pcaIdx );
	uint8_t* baseData = (uint8_t*)( &( ecps->componentData.sbComponentArrays[pcaIdx].sbData[currOffset] ) );
	( *(EntityID*)( baseData ) ) = entityID;
	modifyEntityDirectoryEntry( ecps, entityID, pcaIdx, currOffset );

	va_copy( list, va ); {
		for( size_t i = 0; i < numComponents; ++i ) {
			ComponentID compID = va_arg( list, ComponentID );
			void* compData = va_arg( list, void* );
			size_t compSize = ecps->componentTypes.sbTypes[compID].size;
			
			if( compSize > 0 ) {
				if( compData != NULL ) {
					// have data, copy it
					memcpy( &( baseData[ecps->componentData.sbComponentArrays[pcaIdx].structure.entries[compID].offset] ), compData, compSize );
				} else {
					// no data, zero it out
					memset( &( baseData[ecps->componentData.sbComponentArrays[pcaIdx].structure.entries[compID].offset] ), 0, compSize );
				}
			}
		}
	} va_end( list );
}

static void pushCreateCommand( ECPS* ecps, EntityID entityID, size_t numComponents, va_list va )
{
	va_list list;

	// first gather how much memory we'll need to allocate, should be slightly faster to do a 
	//  single allocation than multiple
	size_t totalSize = sizeof( CreateEntityCommand );
	va_copy( list, va ); {
		for( size_t i = 0; i < numComponents; ++i ) {
			ComponentID compID = va_arg( list, ComponentID );
			va_arg( list, void* );

			totalSize += sizeof( ComponentID );
			totalSize += ecps->componentTypes.sbTypes[compID].size;
		}
	} va_end( list );

	// allocate the space
	uint8_t* currMem = sb_Add( ecps->sbCommandBuffer, totalSize );
	
	// now copy all the data over
	//  first the command specific stuff
	CreateEntityCommand cmd;
	cmd.cmd = CMD_CREATE_ENTITY;
	cmd.id = entityID;
	cmd.numComps = numComponents;
	memcpy( currMem, &cmd, sizeof( CreateEntityCommand ) );
	currMem += sizeof( CreateEntityCommand );

	//  then the components and their data
	va_copy( list, va ); {
		for( size_t i = 0; i < numComponents; ++i ) {
			ComponentID compID = va_arg( list, ComponentID );
			void* compData = va_arg( list, void* );

			memcpy( currMem, &compID, sizeof( ComponentID ) );
			currMem += sizeof( ComponentID );

			size_t compSize = ecps->componentTypes.sbTypes[compID].size;
			if( compSize > 0 ) {
				if( compData != NULL ) {
					// have data, copy it
					memcpy( currMem, compData, compSize );
				} else {
					// no data, zero it out
					memset( currMem, 0, compSize );
				}
			}
			currMem += compSize;
		}
	} va_end( list );
}

// returns past end of command
static uint8_t* runCreateCommand( ECPS* ecps, uint8_t* commandData )
{
	CreateEntityCommand* cmd = (CreateEntityCommand*)commandData;
	uint8_t* data;

	ComponentBitFlags entityBitFlags;

	memset( &entityBitFlags, 0, sizeof( ComponentBitFlags ) );
	data = commandData + sizeof( CreateEntityCommand );
	for( size_t i = 0; i < cmd->numComps; ++i ) {
		ComponentID compID = *( (ComponentID*)( data ) ); data += sizeof( ComponentID );
		data += ecps->componentTypes.sbTypes[compID].size; // skip past the data for now

		if( ecps_ct_IsComponentTypeValid( &( ecps->componentTypes ), compID ) ) {
			ecps_cbf_SetFlagOn( &entityBitFlags, compID );
		} else {
			llog( LOG_DEBUG, "Creating an entity with invalid component at varg position: %i", i );
		}
	}

	ecps_cbf_SetFlagOn( &entityBitFlags, sharedComponent_ID );
	ecps_cbf_SetFlagOn( &entityBitFlags, sharedComponent_Enabled );

	// find or create the packaged array for this entity
	uint32_t pcaIdx = createOrFindPackagedArray( ecps, &entityBitFlags );

	// add the entity to the list
	size_t currOffset = allocateDataForEntity( ecps, pcaIdx );
	uint8_t* baseData = (uint8_t*)( &( ecps->componentData.sbComponentArrays[pcaIdx].sbData[currOffset] ) );
	( *(EntityID*)( baseData ) ) = cmd->id;
	modifyEntityDirectoryEntry( ecps, cmd->id, pcaIdx, currOffset );

	data = commandData + sizeof( CreateEntityCommand );
	for( size_t i = 0; i < cmd->numComps; ++i ) {
		ComponentID compID = *( (ComponentID*)( data ) ); data += sizeof( ComponentID );
		size_t compSize = ecps->componentTypes.sbTypes[compID].size;
		memcpy( &( baseData[ecps->componentData.sbComponentArrays[pcaIdx].structure.entries[compID].offset] ), (void*)data, compSize );
		data += compSize;
	}

	return data;
}

// creates an entity with the associated components, expects the variable argument list to be
//  interleaved { ComponentID id, void* compData } groupings
//  the memory pointed to by compData is copied into the component specified by id for the
//  new entity
//  returns the id of the entity
//  the returned id is 0 if the creation fails
EntityID ecps_CreateEntity( ECPS* ecps, size_t numComponents, ... )
{
	assert( ecps != NULL );

	EntityID entityID = idSet_ClaimID( &( ecps->idSet ) );
	va_list list;

	if( entityID == 0 ) {
		return 0;
	}

	va_start( list, numComponents ); {
		if( ecps->isRunningProcess ) {
			pushCreateCommand( ecps, entityID, numComponents, list );
		} else {
			createEntityVA( ecps, entityID, numComponents, list );
		}
	} va_end( list );

	return entityID;
}

// finds the entity with the given id
bool ecps_GetEntityByID( ECPS* ecps, EntityID entityID, Entity* outEntity )
{
	assert( ecps != NULL );

	// get the packaged component array
	uint32_t idx = idSet_GetIndex( entityID );

	if( idx >= sb_Count( ecps->componentData.sbEntityDirectory ) ) {
		return false;
	}

	int32_t arrayIdx = ecps->componentData.sbEntityDirectory[idx].packedArrayIdx;
	if( arrayIdx < 0 ) {
		return false;
	}

	size_t posOffset = ecps->componentData.sbEntityDirectory[idx].positionOffset;
	PackagedComponentArray* pca = &( ecps->componentData.sbComponentArrays[arrayIdx] );

	// check to make sure the indexed entity found is the entity we're searching for
	EntityID foundID = *( (EntityID*)( &( pca->sbData[posOffset] ) ) );
	if( foundID != entityID ) {
		return false;
	}

	if( outEntity != NULL ) {
		outEntity->id = entityID;
		outEntity->data = (void*)( &( pca->sbData[posOffset] ) );
		outEntity->structure = &( pca->structure );
	}

	return true;
}

static int immediateAddComponentToEntity( ECPS* ecps, Entity* entity, ComponentID componentID, void* data )
{
	ComponentBitFlags oldBitFlags;
	ComponentBitFlags newBitFlags;

	PackageStructure* fromStructure = NULL;
	uint8_t* fromData = NULL;

	PackageStructure* toStructure = NULL;
	uint8_t* toData = NULL;

	int32_t toPackedArrayIndex = -1;

	uint32_t idx = idSet_GetIndex( entity->id );

	if( idx >= sb_Count( ecps->componentData.sbEntityDirectory ) ) {
		return -2;
	}

	// find position in array it's currently stored in
	EntityDirectoryEntry* directoryEntry = &( ecps->componentData.sbEntityDirectory[idx] );
	if( directoryEntry->packedArrayIdx < 0 ) {
		return -3;
	}

	int32_t fromPackedArrayIndex = directoryEntry->packedArrayIdx;
	size_t fromCompArrayPos = directoryEntry->positionOffset;
	assert( fromPackedArrayIndex >= 0 );

	fromData = &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].sbData[fromCompArrayPos] );
	fromStructure = &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].structure );

	EntityID foundID = *( (EntityID*)fromData );
	if( foundID != entity->id ) {
		return -3;
	}

	size_t currOffset = 0;
	// if the entity already has that component, then don't bother adding it
	if( fromStructure->entries[componentID].offset >= 0 ) {
		currOffset = fromCompArrayPos;

		entity->structure = fromStructure;
		entity->data = fromData;
	} else {
		// entity shouldn't have desired component type, copy over to new array, initialize, and update

		// get from bit flags and generate new bit flags
		oldBitFlags = ecps->componentData.sbBitFlags[fromPackedArrayIndex];
		newBitFlags = oldBitFlags;
		ecps_cbf_SetFlagOn( &newBitFlags, componentID );

		toPackedArrayIndex = createOrFindPackagedArray( ecps, &newBitFlags );

		int32_t currArrayIdx = -1;
		currOffset = allocateDataForEntity( ecps, toPackedArrayIndex );
		toData = &( ecps->componentData.sbComponentArrays[toPackedArrayIndex].sbData[currOffset] );
		toStructure = &( ecps->componentData.sbComponentArrays[toPackedArrayIndex].structure );

		// copy over
		//  NOTE: We directly access the fromStructure here as the allocateDataForEntity( ) function can invalidate our old pointer
		entityCopy( ecps, fromData, &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].structure ), toData, toStructure );

		// remove from old array and update entity directory entry
		freeUpDataFromEntity( ecps, fromPackedArrayIndex, fromCompArrayPos );

		// update
		directoryEntry->packedArrayIdx = toPackedArrayIndex;
		directoryEntry->positionOffset = currOffset;

		entity->structure = toStructure;
		entity->data = toData;
	}

	// set the data to use for initialization, as long as data needs to be set
	if( ecps->componentTypes.sbTypes[componentID].size > 0 ) {
		uint8_t* bytes = (uint8_t*)( entity->data );
		if( data != NULL ) {
			// copy the data
			memcpy( &( bytes[ ( entity->structure )->entries[componentID].offset ] ), data, ecps->componentTypes.sbTypes[componentID].size );
		} else {
			// set the data to 0
			memset( &( bytes[ ( entity->structure )->entries[componentID].offset ] ), 0, ecps->componentTypes.sbTypes[componentID].size );
		}
	}

	return 0;
}

static void pushAddComponentCommand( ECPS* ecps, Entity* entity, ComponentID componentID, void* data )
{
	AddComponentCommand cmd;
	cmd.cmd = CMD_ADD_COMPONENT;
	cmd.id = entity->id;
	cmd.compID = componentID;

	size_t compSize = ecps->componentTypes.sbTypes[componentID].size;
	size_t totalSize = sizeof( AddComponentCommand ) + compSize;

	uint8_t* cmdData = sb_Add( ecps->sbCommandBuffer, totalSize );

	memcpy( cmdData, &cmd, sizeof( AddComponentCommand ) );
	cmdData += sizeof( AddComponentCommand );
	memcpy( cmdData, data, compSize );
}

// returns the point in the command buffer after the add component command
static uint8_t* runAddComponentCommand( ECPS* ecps, uint8_t* commandData )
{
	AddComponentCommand* cmd = (AddComponentCommand*)commandData;
	uint8_t* data = commandData + sizeof( AddComponentCommand );

	Entity entity;
	ecps_GetEntityByID( ecps, cmd->id, &entity );
	immediateAddComponentToEntity( ecps, &entity, cmd->compID, (void*)data );

	return ( data + ecps->componentTypes.sbTypes[cmd->compID].size );
}

// adds a component to the specified entity
//  if the entity already has this component this acts like an expensive ecps_GetComponentFromEntity( )
//  returns >= 0 if successful, < 0 if unsuccesful
//  data should be a pointer to the data used to initialize the new component, if it's NULL it will set whatever
//   memory is created for the component to 0
//  modifies the passed in Entity to match the new structure
//  returns whether the addition was a success or not
int ecps_AddComponentToEntity( ECPS* ecps, Entity* entity, ComponentID componentID, void* data )
{
	assert( ecps != NULL );
	assert( entity != NULL );

	if( !ecps_ct_IsComponentTypeValid( &( ecps->componentTypes ), componentID ) ) {
		llog( LOG_DEBUG, "Attempting to add invalid component to entity." );
		return -1;
	}

	if( ecps->isRunningProcess ) {
		pushAddComponentCommand( ecps, entity, componentID, data );
		return 0;
	} else {
		return immediateAddComponentToEntity( ecps, entity, componentID, data );
	}
}

// acts as ecps_AddComponentToEntity( ), but uses an entityID instead of an Entity structure
int ecps_AddComponentToEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID, void* data )
{
	assert( ecps != NULL );

	Entity entity;

	if( !ecps_GetEntityByID( ecps, entityID, &entity ) ) {
		return -4;
	}

	return ecps_AddComponentToEntity( ecps, &entity, componentID, data );
}

static int immediateRemoveComponentFromEntity( ECPS* ecps, Entity* entity, ComponentID componentID )
{
	ComponentBitFlags oldBitFlags;
	ComponentBitFlags newBitFlags;

	PackageStructure* fromStructure = NULL;
	uint8_t* fromData = NULL;

	PackageStructure* toStructure = NULL;
	uint8_t* toData = NULL;

	int32_t toPackedArrayIndex = -1;

	uint32_t idx = idSet_GetIndex( entity->id );

	size_t entityDirectoryCount = sb_Count( ecps->componentData.sbEntityDirectory );
	if( idx >= sb_Count( ecps->componentData.sbEntityDirectory ) ) {
		return -2;
	}

	// find position in array it's currently stored in
	EntityDirectoryEntry* directoryEntry = &( ecps->componentData.sbEntityDirectory[idx] );
	if( directoryEntry->packedArrayIdx < 0 ) {
		return -3;
	}

	int32_t fromPackedArrayIndex = directoryEntry->packedArrayIdx;
	size_t fromCompArrayPos = directoryEntry->positionOffset;
	assert( fromPackedArrayIndex >= 0 );

	fromData = &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].sbData[fromCompArrayPos] );
	fromStructure = &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].structure );

	EntityID foundID = *( (EntityID*)fromData );
	if( foundID != entity->id ) {
		return -3;
	}

	// no reason to remove the entity
	if( fromStructure->entries[componentID].offset < 0 ) {
		return 0;
	}

	// get from bit flags and generate new bit flags
	oldBitFlags = ecps->componentData.sbBitFlags[fromPackedArrayIndex];
	newBitFlags = oldBitFlags;
	ecps_cbf_SetFlagOff( &newBitFlags, componentID );

	// add spot to new array
	toPackedArrayIndex = createOrFindPackagedArray( ecps, &newBitFlags );
	size_t currOffset = allocateDataForEntity( ecps, toPackedArrayIndex );

	toData = &( ecps->componentData.sbComponentArrays[toPackedArrayIndex].sbData[currOffset] );
	toStructure = &( ecps->componentData.sbComponentArrays[toPackedArrayIndex].structure );

	// copy over, because createOrFindPackagedArray( ) can invalidate the fromStructure pointer we just access the structure directly
	entityCopy( ecps, fromData, &( ecps->componentData.sbComponentArrays[fromPackedArrayIndex].structure ), toData, toStructure );

	// remove from old array and update entity directory entry
	freeUpDataFromEntity( ecps, fromPackedArrayIndex, fromCompArrayPos );

	directoryEntry->packedArrayIdx = toPackedArrayIndex;
	directoryEntry->positionOffset = currOffset;

	entity->structure = toStructure;
	entity->data = toData;

	return 0;
}

static void pushRemoveComponentCommand( ECPS* ecps, Entity* entity, ComponentID componentID )
{
	RemoveComponentCommand cmd;
	cmd.cmd = CMD_REMOVE_COMPONENT;
	cmd.id = entity->id;
	cmd.compID = componentID;

	uint8_t* cmdData = sb_Add( ecps->sbCommandBuffer, sizeof( RemoveComponentCommand ) );

	memcpy( cmdData, &cmd, sizeof( RemoveComponentCommand ) );
}

static uint8_t* runRemoveComponentCommand( ECPS* ecps, uint8_t* commandData )
{
	RemoveComponentCommand* cmd = (RemoveComponentCommand*)commandData;

	Entity entity;
	ecps_GetEntityByID( ecps, cmd->id, &entity );
	immediateRemoveComponentFromEntity( ecps, &entity, cmd->compID );

	return ( commandData + sizeof( RemoveComponentCommand ) );
}

// removed a component from the specified entity
//  if the entity doesn't have this component it will do nothing
//  returns >= 0 if successful, < 0 if unsuccesful
//  modifies the passed in Entity to match the new structure
int ecps_RemoveComponentFromEntity( ECPS* ecps, Entity* entity, ComponentID componentID )
{
	assert( ecps != NULL );
	assert( entity != NULL );

	if( ecps->isRunningProcess ) {
		pushRemoveComponentCommand( ecps, entity, componentID );
		return 0;
	} else {
		return immediateRemoveComponentFromEntity( ecps, entity, componentID );
	}
}

int ecps_RemoveComponentFromEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID )
{
	assert( ecps != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( ecps, entityID, &entity ) ) {
		return -4;
	}

	return ecps_RemoveComponentFromEntity( ecps, &entity, componentID );
}

bool ecps_DoesEntityHaveComponent( const Entity* entity, ComponentID componentID )
{
	assert( entity != NULL );

	return ( entity->structure->entries[componentID].offset >= 0 );
}

bool ecps_DoesEntityHaveComponentByID( ECPS* ecps, EntityID entityID, ComponentID componentID )
{
	assert( ecps != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( ecps, entityID, &entity ) ) {
		return false;
	}

	return ecps_DoesEntityHaveComponent( &entity, componentID );
}

// gets the component from the entity, puts a pointer to it in outData
//  puts in NULL if the entity doesn't have that component
bool ecps_GetComponentFromEntity( const Entity* entity, ComponentID componentID, void** outData )
{
	assert( entity != NULL );
	assert( outData != NULL );

	if( componentID == INVALID_COMPONENT_ID ) {
		llog( LOG_ERROR, "Attempting to retrieve an invalid component type from entity %08X", entity->id );
		(*outData) = NULL;
		return false;
	}

	if( entity->structure->entries[componentID].offset < 0 ) {
		(*outData) = NULL;
		return false;
	}

	uint8_t* bytes = (uint8_t*)( entity->data );
	(*outData) = &( bytes[ ( entity->structure )->entries[componentID].offset ] );
	return true;
}

bool ecps_GetComponentFromEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID, void** outData )
{
	assert( ecps != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( ecps, entityID, &entity ) ) {
		return false;
	}

	return ecps_GetComponentFromEntity( &entity, componentID, outData );
}

bool ecps_GetEntityAndComponentByID( ECPS* ecps, EntityID entityID, ComponentID componentID, Entity* outEntity, void** outData )
{
	assert( ecps != NULL );
	assert( outEntity != NULL );

	if( !ecps_GetEntityByID( ecps, entityID, outEntity ) ) {
		return false;
	}

	return ecps_GetComponentFromEntity( outEntity, componentID, outData );
}

static void immediateDestroyEntity( ECPS* ecps, EntityID entityID )
{
	removeEntityFromArray( ecps, entityID );
	idSet_ReleaseID( &( ecps->idSet ), entityID );
}

static void pushDestroyEntityCommand( ECPS* ecps, EntityID id )
{
	DestroyEntityCommand cmd;
	cmd.cmd = CMD_DESTROY_ENTITY;
	cmd.id = id;
	
	uint8_t* cmdData = sb_Add( ecps->sbCommandBuffer, sizeof( DestroyEntityCommand ) );

	memcpy( cmdData, &cmd, sizeof( DestroyEntityCommand ) );
}

static uint8_t* runDestroyEntityCommand( ECPS* ecps, uint8_t* commandData )
{
	DestroyEntityCommand* cmd = (DestroyEntityCommand*)commandData;

	immediateDestroyEntity( ecps, cmd->id );

	return ( commandData + sizeof( DestroyEntityCommand ) );
}

void ecps_DestroyEntity( ECPS* ecps, const Entity* entity )
{
	assert( entity != NULL );
	ecps_DestroyEntityByID( ecps, entity->id );
}

void ecps_DestroyEntityByID( ECPS* ecps, EntityID entityID )
{
	assert( ecps != NULL );

	if( ecps->isRunningProcess ) {
		pushDestroyEntityCommand( ecps, entityID );
	} else {
		immediateDestroyEntity( ecps, entityID );
	}
}

void ecps_DestroyAllEntities( ECPS* ecps )
{
	assert( ecps != NULL );
	assert( !( ecps->isRunningProcess ) );

	idSet_Clear( &( ecps->idSet ) );

	// clear out all the entity data
	sb_Release( ecps->componentData.sbBitFlags );
	ecps->componentData.sbBitFlags = NULL;

	for( size_t i = 0; i < sb_Count( ecps->componentData.sbComponentArrays ); ++i ) {
		sb_Release( ecps->componentData.sbComponentArrays[i].sbData );
	}
	sb_Release( ecps->componentData.sbComponentArrays );
	ecps->componentData.sbComponentArrays = NULL;

	sb_Release( ecps->componentData.sbEntityDirectory );
	ecps->componentData.sbEntityDirectory = NULL;
}

// lists all entities and the type of components they have
void ecps_DumpAllEntities( ECPS* ecps, const char* tag )
{
	if( tag == NULL ) {
		llog( LOG_DEBUG, "Starting entity dump:" );
	} else {
		llog( LOG_DEBUG, "Starting entity dump (%s):", tag );
	}

	char* sbTypeList = NULL;
	for( EntityID id = idSet_GetFirstValidID( &( ecps->idSet ) ); id != INVALID_ENTITY_ID; id = idSet_GetNextValidID( &( ecps->idSet ), id ) ) {
		Entity entity;
		
		// we should always be able to find the entity
		assert( ecps_GetEntityByID( ecps, id, &entity ) ); 

		bool isFirst = true;
		sb_Clear( sbTypeList );
		for( ComponentID compID = 0; compID < sb_Count( ecps->componentTypes.sbTypes ); ++compID ) {
			if( ecps_DoesEntityHaveComponent( &entity, compID ) ) {
				char* name = ecps->componentTypes.sbTypes[compID].name;
				size_t len = strnlen( name, 32 );

				if( isFirst ) {
					isFirst = false;
				} else {
					sb_Push( sbTypeList, ',' );
					sb_Push( sbTypeList, ' ' );
				}

				for( size_t c = 0; c < len; ++c ) {
					sb_Push( sbTypeList, name[c] );
				}
			}
		}
		sb_Push( sbTypeList, '\0' );

		llog( LOG_DEBUG, "  0x%08X: %s", entity.id, sbTypeList );//*/
	}

	sb_Release( sbTypeList );
}