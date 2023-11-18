#include "ecps_serialization.h"

#include <SDL.h>
#include <stdio.h>

#include "System/ECPS/entityComponentProcessSystem.h"
#include "System/ECPS/ecps_componentTypes.h"

#include "System/platformLog.h"
#include "System/memory.h"
#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"
#include "Utils/idSet.h"

//***************************************************************

typedef struct {
	size_t pos;
	size_t size;
	uint8_t* data;
} ByteBuffer;

void buffer_Init( ByteBuffer* buffer )
{
	SDL_assert( buffer != NULL );

	buffer->data = NULL;
	buffer->size = 0;
	buffer->pos = 0;
}

void buffer_Set( ByteBuffer* buffer, void* data, size_t size )
{
	SDL_assert( buffer != NULL );

	buffer->data = data;
	buffer->size = size;
	buffer->pos = 0;
}

void buffer_Clean( ByteBuffer* buffer )
{
	assert( buffer != NULL );

	free( buffer->data );
	buffer->data = NULL;
	buffer->size = 0;
	buffer->pos = 0;
}

void buffer_Seek( ByteBuffer* buffer, size_t pos )
{
	assert( buffer != NULL );
	assert( pos < buffer->size );

	buffer->pos = pos;
}

//***************************************************************

bool serializationReader( struct cmp_ctx_s* ctx, void* data, size_t limit )
{
	ByteBuffer* buffer = (ByteBuffer*)( ctx->buf );
	if( buffer->pos + limit > buffer->size ) {
		return false;
	}
	SDL_memcpy( data, buffer->data + buffer->pos, limit );
	buffer->pos += limit;
	return true;
}

bool serializationSkipper( struct cmp_ctx_s* ctx, size_t count )
{
	// return value of skipper isn't used, so can't tell what the correct return value is for what state
	ByteBuffer* buffer = (ByteBuffer*)( ctx->buf );
	if( buffer->pos + count >= buffer->size ) {
		return false;
	}
	buffer->pos += count;
	return true;
}

size_t serializationWriter( struct cmp_ctx_s* ctx, const void* data, size_t count )
{
	ByteBuffer* buffer = (ByteBuffer*)( ctx->buf );
	if( buffer->pos + count >= buffer->size ) {
		// try to allocate some extra data
		size_t growth = ( buffer->pos + count ) - buffer->size;
		buffer->data = mem_Resize( buffer->data, buffer->size + growth );
		if( buffer->data == NULL ) {
			return false;
		}
		buffer->size = buffer->size + growth;
	}
	SDL_memcpy( buffer->data + buffer->pos, data, count );
	buffer->pos += count;
	return true;
}

//***************************************************************

void ecps_InitSerialized( SerializedECPS* serializedECPS )
{
	serializedECPS->sbCompInfos = NULL;
	serializedECPS->sbEntityInfos = NULL;
}

void ecps_CleanSerialized( SerializedECPS* serializedECPS )
{
	for( size_t i = 0; i < sb_Count( serializedECPS->sbEntityInfos ); ++i ) {
		for( size_t a = 0; a < sb_Count( serializedECPS->sbEntityInfos[i].sbCompData ); ++a ) {
			mem_Release( serializedECPS->sbEntityInfos[i].sbCompData[a].data );
		}
	}
	sb_Release( serializedECPS->sbCompInfos );
	sb_Release( serializedECPS->sbEntityInfos );
}

static SerializedComponentInfo* generateSerializedComponentInfo( const ECPS* ecps )
{
	SerializedComponentInfo* sbInfos = NULL;

	// loop through each component in the ecps and create the serialized component infos for them
	ComponentType* sbTypes = ecps->componentTypes.sbTypes;

	SDL_assert( sb_Count( sbTypes ) <= (size_t)UINT32_MAX );
	SerializedComponentInfo testInfo;
	SDL_assert( ARRAY_SIZE( sbTypes[0].name ) == ARRAY_SIZE( testInfo.externalID ) );

	for( size_t i = 0; i < sb_Count( sbTypes ); ++i ) {
		SerializedComponentInfo newInfo;

		newInfo.internalID = (uint32_t)i;

		SDL_memcpy( newInfo.externalID, sbTypes[i].name, ARRAY_SIZE( sbTypes[i].name ) );
		newInfo.externalVersion = sbTypes[i].version;

		newInfo.used = false;

		sb_Push( sbInfos, newInfo );
	}

	return sbInfos;
}

static SerializedEntityInfo* generateSerializedEntityInfo( const ECPS* ecps )
{
	SerializedEntityInfo* sbInfos = NULL;

	// loop through each entity and do some initial set up, create the internal id
	//  we'll use 0 as an invalid id
	uint32_t internalID = 1;
	for( EntityID id = idSet_GetFirstValidID( &( ecps->idSet ) ); id != INVALID_ENTITY_ID; ++internalID, id = idSet_GetNextValidID( &( ecps->idSet ), id ) )
	{

		SerializedEntityInfo newInfo;
		newInfo.ecpsEntityID = id;
		newInfo.internalID = internalID;
		newInfo.sbCompData = NULL;

		sb_Push( sbInfos, newInfo );
	}

	return sbInfos;
}

static void generateSerializedComponents( const ECPS* ecps, SerializedEntityInfo* sbEntityInfos, SerializedComponentInfo* sbComponentInfos )
{
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {
		EntityID entityID = sbEntityInfos[i].ecpsEntityID;

		// get all components in this entity
		for( ComponentID compID = 0; compID < sb_Count( ecps->componentTypes.sbTypes ); ++compID ) {

			if( compID == sharedComponent_ID ) {
				// special case, ignore the id component
				continue;
			}

			void* compData = NULL;
			if( ecps_GetComponentFromEntityByID( ecps, entityID, compID, &compData ) ) {

				// find internal id for component
				SerializedComponent serializedComp;
				for( size_t x = 0; x < sb_Count( sbComponentInfos ); ++x ) {
					if( SDL_strcmp( sbComponentInfos[x].externalID, ecps->componentTypes.sbTypes[compID].name ) == 0 ) {
						serializedComp.internalCompID = sbComponentInfos[x].internalID;

						// make the component type as used, later when we save it out we can cull all those that don't have this flag set
						sbComponentInfos[x].used = true;

						break;
					}
				}

				// store the data, we're not cleaning up the buffer after because we're relying on it's allocated buffer to store data
				ByteBuffer buffer;
				buffer_Init( &buffer );

				SerializeComponent serialize = ecps_GetComponentSerializtionFunction( ecps, compID );
				if( serialize != NULL ) {
					cmp_ctx_t ctx;
					cmp_init( &ctx, &buffer, serializationReader, serializationSkipper, serializationWriter );

					if( !serialize( &ctx, compData, sbEntityInfos ) ) {
						llog( LOG_ERROR, "Error serializing component %s in entity %0x: %s", ecps->componentTypes.sbTypes[compID].name, entityID, cmp_strerror( &ctx ) );
					}
				}

				serializedComp.data = buffer.data;
				serializedComp.dataSize = (uint32_t)buffer.pos;
				sb_Push( sbEntityInfos[i].sbCompData, serializedComp );
			}
		}
	}
}

void ecps_GenerateSerializedECPS( const ECPS* ecps, SerializedECPS* serializedECPS )
{
	ASSERT_AND_IF( ecps != NULL ) return;
	ASSERT_AND_IF( serializedECPS != NULL ) return;
	ASSERT_AND_IF( !ecps->isRunningProcess ) return;

	serializedECPS->sbCompInfos = generateSerializedComponentInfo( ecps );
	serializedECPS->sbEntityInfos = generateSerializedEntityInfo( ecps );
	generateSerializedComponents( ecps, serializedECPS->sbEntityInfos, serializedECPS->sbCompInfos );
}

static bool deserializeMapComponentTypes( const ECPS* ecps, SerializedComponentInfo* sbComponentInfos )
{
	// make sure all the components in the stored sbComponentInfos exist
	for( size_t i = 0; i < sb_Count( sbComponentInfos ); ++i ) {
		// may have been from a set of component infos just created, if it's not used then skip past it
		if( !sbComponentInfos[i].used ) continue;

		// make sure the external id and version are the same
		bool found = false;
		for( ComponentID compID = 0; compID < sb_Count( ecps->componentTypes.sbTypes ) && !found; ++compID ) {
			if( SDL_strcmp( sbComponentInfos[i].externalID, ecps->componentTypes.sbTypes[compID].name ) == 0 ) {
				found = true;
				// matching id, these should be unique
				if( sbComponentInfos[i].externalVersion != ecps->componentTypes.sbTypes[compID].version ) {
					llog( LOG_ERROR, "Component versions for type %s don't match.", sbComponentInfos[i].externalID );
					return false;
				} else {
					// store the component id for later use
					sbComponentInfos[i].ecpsComponentID = compID;
				}
			}
		}

		if( !found ) {
			llog( LOG_ERROR, "Unable to find component of type %s in ECPS.", sbComponentInfos[i].externalID );
			return false;
		}
	}

	return true;
}

static bool deserializeCreateEntities( ECPS* ecps, SerializedEntityInfo* sbEntityInfos )
{
	// initialize to invalid entities
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {
		sbEntityInfos[i].ecpsEntityID = INVALID_ENTITY_ID;
	}

	// create all the serialized entities, only create the entities, the components will be added later
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {
		sbEntityInfos[i].ecpsEntityID = ecps_CreateEntity( ecps, 0 );
		if( sbEntityInfos[i].ecpsEntityID == INVALID_ENTITY_ID ) {
			llog( LOG_ERROR, "Unable to create entity." );
			return false;
		}

		// remove the enabled component, will be added again in a later step if it's needed
		ecps_RemoveComponentFromEntityByID( ecps, sbEntityInfos[i].ecpsEntityID, sharedComponent_Enabled );
	}

	return true;
}

static bool deserializeEntityComponents( ECPS* ecps, SerializedEntityInfo* sbEntityInfos, SerializedComponentInfo* sbComponentInfos )
{
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {

		// get all components associated with the entity and deserialize them
		for( size_t a = 0; a < sb_Count( sbEntityInfos[i].sbCompData ); ++a ) {
			uint32_t internalCompID = sbEntityInfos[i].sbCompData[a].internalCompID;

			// get the component id
			SerializedComponentInfo info;
			info.ecpsComponentID = INVALID_COMPONENT_ID;
			for( size_t x = 0; x < sb_Count( sbComponentInfos ); ++x ) {
				if( sbComponentInfos[x].internalID == sbEntityInfos[i].sbCompData[a].internalCompID ) {
					info = sbComponentInfos[x];
					break;
				}
			}

			ASSERT_AND_IF( info.ecpsComponentID != INVALID_COMPONENT_ID ) return false;

			// add the component, then get back the data that has been allocated for it
			//  sending in NULL as the data when it's expecting something will just zero it out
			ecps_AddComponentToEntityByID( ecps, sbEntityInfos[i].ecpsEntityID, info.ecpsComponentID, NULL );
			void* compData = NULL;
			if( !ecps_GetComponentFromEntityByID( ecps, sbEntityInfos[i].ecpsEntityID, info.ecpsComponentID, &compData ) ) {
				llog( LOG_ERROR, "Error creating component %s for deserialized entity.", info.externalID );
				return false;
			}

			DeserializeComponent deserialize = ecps->componentTypes.sbTypes[info.ecpsComponentID].deserialize;
			if( deserialize != NULL ) {
				// create the cmp context to read from
				cmp_ctx_t cmp;
				ByteBuffer dataBuffer;
				buffer_Set( &dataBuffer, sbEntityInfos[i].sbCompData[a].data, sbEntityInfos[i].sbCompData[a].dataSize );
				cmp_init( &cmp, &dataBuffer, serializationReader, serializationSkipper, serializationWriter );

				if( dataBuffer.size > 0 ) {
					deserialize( &cmp, compData, sbEntityInfos );
				}
			}
		}
	}
	
	return true;
}

void ecps_CreateEntitiesFromSerializedComponents( ECPS* ecps, SerializedECPS* serializedECPS )
{
	ASSERT_AND_IF( ecps != NULL ) return;
	ASSERT_AND_IF( serializedECPS != NULL ) return;
	ASSERT_AND_IF( !ecps->isRunningProcess ) return;

	// make sure the components exist, mapping them from the ids to componentIDs
	if( !deserializeMapComponentTypes( ecps, serializedECPS->sbCompInfos ) ) {
		return;
	}

	// create the entities, don't create the components in them yet to make sure entity references in components are valid
	if( !deserializeCreateEntities( ecps, serializedECPS->sbEntityInfos ) ) {
		// destroy all entities that were created
		for( size_t i = 0; i < sb_Count( serializedECPS->sbEntityInfos ); ++i ) {
			if( serializedECPS->sbEntityInfos[i].ecpsEntityID != INVALID_ENTITY_ID ) {
				ecps_DestroyEntityByID( ecps, serializedECPS->sbEntityInfos[i].ecpsEntityID );
			}
		}
		return;
	}

	// deserialize the components and add them to the entities
	if( !deserializeEntityComponents( ecps, serializedECPS->sbEntityInfos, serializedECPS->sbCompInfos ) ) {
		// destroy all entities that were created
		for( size_t i = 0; i < sb_Count( serializedECPS->sbEntityInfos ); ++i ) {
			if( serializedECPS->sbEntityInfos[i].ecpsEntityID != INVALID_ENTITY_ID ) {
				ecps_DestroyEntityByID( ecps, serializedECPS->sbEntityInfos[i].ecpsEntityID );
			}
		}
		return;
	}
}

uint32_t ecps_GetLocalIDFromSerializeEntityID( SerializedEntityInfo* sbEntityInfos, EntityID id )
{
	// returns 0 if it isn't able to find the entity with the id
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {
		if( sbEntityInfos[i].ecpsEntityID == id ) {
			return sbEntityInfos[i].internalID;
		}
	}

	return 0;
}

EntityID ecps_GetEntityIDfromSerializedLocalID( SerializedEntityInfo* sbEntityInfos, uint32_t id )
{
	// returns INVALID_ENTITY_ID if it isn't able to find the entity with the local id
	for( size_t i = 0; i < sb_Count( sbEntityInfos ); ++i ) {
		if( sbEntityInfos[i].internalID == id ) {
			return sbEntityInfos[i].ecpsEntityID;
		}
	}

	return INVALID_ENTITY_ID;
}

//***************************************************************

bool serializationFileReader( struct cmp_ctx_s* ctx, void* data, size_t limit )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWread( rwops, data, limit, 1 ) != 1 ) {
		llog( LOG_ERROR, "Error reading from file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

bool serializationFileSkipper( struct cmp_ctx_s* ctx, size_t count )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWseek( rwops, (Sint64)count, RW_SEEK_CUR ) == -1 ) {
		llog( LOG_ERROR, "Error seeking in file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

size_t serializationFileWriter( struct cmp_ctx_s* ctx, const void* data, size_t count )
{
	SDL_RWops* rwops = (SDL_RWops*)( ctx->buf );
	if( SDL_RWwrite( rwops, data, count, 1 ) != 1 ) {
		llog( LOG_ERROR, "Error writing to file: %s", SDL_GetError( ) );
		return false;
	}
	return true;
}

//***************************************************************

// save and load to external files
bool ecps_SaveSerializedECPS( const char* fileName, SerializedECPS* serializedECPS )
{
	ASSERT_AND_IF( fileName != NULL ) return false;
	ASSERT_AND_IF( serializedECPS != NULL ) return false;

	// TODO: handle not overwriting an existing file if the writing fails
	cmp_ctx_t cmp;
	SDL_RWops* rwopsFile = openRWopsCMPFile( fileName, "wb", &cmp );
	if( rwopsFile == NULL ) {
		return false;
	}

	cmp_init( &cmp, rwopsFile, serializationFileReader, serializationFileSkipper, serializationFileWriter );
	bool done = false;

	// find the number of used components
	uint32_t numCompInfos = 0;
	for( uint32_t i = 0; i < sb_Count( serializedECPS->sbCompInfos ); ++i ) {
		if( serializedECPS->sbCompInfos[i].used ) {
			++numCompInfos;
		}
	}

	// then write them out
	if( !cmp_write_array( &cmp, numCompInfos ) ) {
		llog( LOG_ERROR, "Unable to write component type count: %s", cmp_strerror( &cmp ) );
		goto clean_up;
	}
	for( size_t i = 0; i < sb_Count( serializedECPS->sbCompInfos ); ++i ) {
		if( !serializedECPS->sbCompInfos[i].used ) continue;

		if( !cmp_write_u32( &cmp, serializedECPS->sbCompInfos[i].internalID ) ) {
			llog( LOG_ERROR, "Unable to write component internal id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
		if( !cmp_write_u32( &cmp, serializedECPS->sbCompInfos[i].externalVersion ) ) {
			llog( LOG_ERROR, "Unable to write component external version: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		uint32_t strlen = (uint32_t)SDL_strlen( serializedECPS->sbCompInfos[i].externalID );
		if( !cmp_write_str( &cmp, serializedECPS->sbCompInfos[i].externalID, strlen ) ) {
			llog( LOG_ERROR, "Unable to write component external id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
	}

	// write out the entity info
	uint32_t numEntities = (uint32_t)sb_Count( serializedECPS->sbEntityInfos );
	if( !cmp_write_array( &cmp, numEntities ) ) {
		llog( LOG_ERROR, "Unable to write number of entities: %s", cmp_strerror( &cmp ) );
		goto clean_up;
	}
	for( uint32_t i = 0; i < numEntities; ++i ) {
		if( !cmp_write_u32( &cmp, serializedECPS->sbEntityInfos[i].internalID ) ) {
			llog( LOG_ERROR, "Unable to write internal entity id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		// write out the entity components
		uint32_t numComponents = (uint32_t)sb_Count( serializedECPS->sbEntityInfos[i].sbCompData );
		if( !cmp_write_array( &cmp, numComponents ) ) {
			llog( LOG_ERROR, "Unable to write number of components in entity: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
		for( size_t a = 0; a < numComponents; ++a ) {
			SerializedComponent* comp = &( serializedECPS->sbEntityInfos[i].sbCompData[a] );
			if( !cmp_write_u32( &cmp, comp->internalCompID ) ) {
				llog( LOG_ERROR, "Unable to write internal component id for entity: %s", cmp_strerror( &cmp ) );
				goto clean_up;
			}
			if( !cmp_write_bin( &cmp, comp->data, comp->dataSize ) ) {
				llog( LOG_ERROR, "Unable to write component data for entity: %s", cmp_strerror( &cmp ) );
				goto clean_up;
			}
		}
	}
	done = true;

clean_up:
	if( SDL_RWclose( rwopsFile ) < 0 ) {
		llog( LOG_ERROR, "Error flushing out file %s: %s", fileName, SDL_GetError( ) );
		done = false;
	}
	if( !done ) {
		// delete the file if we didn't finish the writing
		remove( fileName );
	}

	return done;
}

bool ecps_LoadSerializedECPS( const char* fileName, SerializedECPS* serializedECPS )
{
	ASSERT_AND_IF( fileName != NULL ) return false;
	ASSERT_AND_IF( serializedECPS != NULL ) return false;
	ASSERT_AND_IF( serializedECPS->sbCompInfos == NULL ) return false;
	ASSERT_AND_IF( serializedECPS->sbEntityInfos == NULL ) return false;

	cmp_ctx_t cmp;
	SDL_RWops* rwopsFile = openRWopsCMPFile( fileName, "rb", &cmp );
	if( rwopsFile == NULL ) {
		return false;
	}

	bool done = false;
	// read the component infos
	uint32_t numCompInfos = 0;
	if( !cmp_read_array( &cmp, &numCompInfos ) ) {
		llog( LOG_ERROR, "Unable to read component type count: %s", cmp_strerror( &cmp ) );
		goto clean_up;
	}
	for( uint32_t i = 0; i < numCompInfos; ++i ) {
		SerializedComponentInfo compInfo;
		if( !cmp_read_u32( &cmp, &( compInfo.internalID ) ) ) {
			llog( LOG_ERROR, "Unable to read component internal id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
		if( !cmp_read_u32( &cmp, &( compInfo.externalVersion ) ) ) {
			llog( LOG_ERROR, "Unable to read component external version: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		uint32_t strSize = (uint32_t)ARRAY_SIZE( compInfo.externalID );
		if( !cmp_read_str( &cmp, &( compInfo.externalID[0] ), &strSize ) ) {
			llog( LOG_ERROR, "Unable to read component external id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		sb_Push( serializedECPS->sbCompInfos, compInfo );
	}

	// read the entity infos
	uint32_t numEntities;
	if( !cmp_read_array( &cmp, &numEntities ) ) {
		llog( LOG_ERROR, "Unable to read number of entities: %s", cmp_strerror( &cmp ) );
		goto clean_up;
	}
	for( uint32_t i = 0; i < numEntities; ++i ) {
		SerializedEntityInfo entityInfo;
		entityInfo.sbCompData = NULL;

		if( !cmp_read_u32( &cmp, &(entityInfo.internalID) ) ) {
			llog( LOG_ERROR, "Unable to read internal entity id: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		uint32_t numComponents = 0;
		if( !cmp_read_array( &cmp, &numComponents ) ) {
			llog( LOG_ERROR, "Unable to read number of components in entity: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
		for( uint32_t a = 0; a < numComponents; ++a ) {
			SerializedComponent comp;
			if( !cmp_read_u32( &cmp, &( comp.internalCompID ) ) ) {
				llog( LOG_ERROR, "Unable to read internal component id for entity: %s", cmp_strerror( &cmp ) );
				goto clean_up;
			}

			if( !cmp_read_bin_size( &cmp, &( comp.dataSize ) ) ) {
				llog( LOG_ERROR, "Unable to read component data size for entity: %s", cmp_strerror( &cmp ) );
				goto clean_up;
			}

			comp.data = mem_Allocate( comp.dataSize );
			if( ( comp.data == NULL ) && ( comp.dataSize > 0 ) ) {
				llog( LOG_ERROR, "Unable to allocate component data entity." );
				goto clean_up;
			}
			if( ( comp.dataSize > 0 ) && !cmp.read( &cmp, comp.data, comp.dataSize ) ) {
				llog( LOG_ERROR, "Unable to read component data for entity: %s", cmp_strerror( &cmp ) );
				goto clean_up;
			}

			sb_Push( entityInfo.sbCompData, comp );
		}

		sb_Push( serializedECPS->sbEntityInfos, entityInfo );
	}
	done = true;

clean_up:
	if( SDL_RWclose( rwopsFile ) < 0 ) {
		llog( LOG_ERROR, "Error flushing out file %s: %s", fileName, SDL_GetError( ) );
	}

	if( !done ) {
		// if we failed clean up anything that was allocated
		ecps_CleanSerialized( serializedECPS );
	}

	return done;
}