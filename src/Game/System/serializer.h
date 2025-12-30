#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <stdint.h>
#include <stdbool.h>

#include "System/ECPS/ecps_trackedCallbacks.h"
#include "Graphics/gfx_commonDataTypes.h"
#include "tween.h"

typedef struct EntityAccessor  {
	void* ctx; // extra things needed for accessing the entity
	bool ( *getEntityID )( struct EntityAccessor* a, uint32_t localID, EntityID* outID );
	bool ( *getLocalID )( struct EntityAccessor* a, EntityID id, uint32_t* outID );
} EntityAccessor;

// want one interface for all our serialization, whether it's out to a file or a script interface
typedef struct Serializer{
	void* ctx; // will point to whatever is needed by the appropriate serializer

	bool ( *startStructure )( struct Serializer* s, const char* name );
	bool ( *endStructure )( struct Serializer* s, const char* name );
	bool ( *s64 )( struct Serializer* s, const char* name, int64_t* d );
	bool ( *cString )( struct Serializer* s, const char* name, char** str ); // we're assuming str is allocated on the stack using mem_Allocate
	bool ( *cStrBuffer )( struct Serializer* s, const char* name, char* str, size_t bufferSize ); // we're assuming str is an already allocated buffer
	bool ( *u32 )( struct Serializer* s, const char* name, uint32_t* i );
	bool ( *s8 )( struct Serializer* s, const char* name, int8_t* c );
	bool ( *flt )( struct Serializer* s, const char* name, float* f );
	bool ( *u64 )( struct Serializer* s, const char* name, uint64_t* u );
	bool ( *s32 )( struct Serializer* s, const char* name, int32_t* i );
	bool ( *boolean )( struct Serializer* s, const char* name, bool* b );
	bool ( *trackedECPSCallback )( struct Serializer* s, const char* name, TrackedCallback* c );
	bool ( *trackedEaseFunc )( struct Serializer* s, const char* name, EaseFunc* e );
	bool ( *imageID )( struct Serializer* s, const char* name, ImageID* imgID );
	bool ( *entityID )( struct Serializer* s, const char* name, uint32_t* id );

	// TODO: figure out how to do stretchy buffers with this, they don't like working with void pointers
	//  write would just need to write out the size, read would need to resize the array to appropriate size
	//bool ( *stretchyBuffer )( struct Serializer* s, const char* name, void** sb, size_t elementSize );

	//  NOTE: This only writes out the size of the array, you're then expected to know how to loop through the elements of it
	bool ( *arraySize )( struct Serializer* s, const char* name, uint32_t* size );

	EntityAccessor entityAccessor;
} Serializer;

// these don't allocate any memory, so it's safe to just discard the Serializer
void serializer_CreateWriteCmp( void* ctx, Serializer* outSerializer );
void serializer_CreateReadCmp( void* ctx, Serializer* outSerializer );

void serializer_CreateWriteLua( void* ctx, Serializer* outSerializer );
void serializer_CreateReadLua( void* ctx, Serializer* outSerializer );

void serializer_SetRawEntityID( Serializer* serializer ); // this is the default, don't need to call it
void serializer_SetEntityInfo( void* ctx, Serializer* serializer );

// helper macros
#define SERIALIZE_CHECK( call, baseType, entryName, onFail ) if( !call ) { llog( LOG_ERROR, "Issue serializing %s in %s.", (entryName), (baseType) ); onFail; }
#define SERIALIZE_ENUM( serializerPtr, baseType, entryName, access, enumType, onFail ) { \
		int32_t v = (int32_t)access; \
		SERIALIZE_CHECK( (serializerPtr)->s32( (serializerPtr), entryName, &(v) ), baseType, entryName, onFail ); \
		access = (enumType)(v); }

#endif // inclusion guard
