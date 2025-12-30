#include "serializer.h"

#include <float.h>

#include "Utils/helpers.h"
#include "System/luaInterface.h"
#include "Others/cmp.h"
#include "System/ECPS/ecps_trackedCallbacks.h"
#include "Graphics/images.h"
#include "ECPS/ecps_fileSerialization.h"

#pragma region CMP SERIALIZERS
//**********************************
// cmp serializer
#define CMP_STANDARD_START \
	ASSERT_AND_IF_NOT( s != NULL ) return false; \
	ASSERT_AND_IF_NOT( s->ctx != NULL ) return false; \
	cmp_ctx_t* cmp = (cmp_ctx_t*)(s->ctx);

#pragma region CMP WRITER
static bool cmpSerializer_startWriteStructure( struct Serializer* s, const char* name )
{
	// nothing
	return true;
}

static bool cmpSerializer_endWriteStructure( struct Serializer* s, const char* name )
{
	// nothing
	return true;
}

static bool cmpSerializer_writeInteger( struct Serializer* s, const char* name, int64_t* d )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( d != NULL ) return false;
	return cmp_write_integer( cmp, *d );
}

static bool cmpSerializer_writeStr( struct Serializer* s, const char* name, char** str )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( str != NULL ) return false;
	size_t size = SDL_strlen( *str );
	return cmp_write_str( cmp, *str, (uint32_t)size );
}

static bool cmpSerializer_writeStrBuffer( struct Serializer* s, const char* name, char* str, size_t bufferSize )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( str != NULL ) return false;
	size_t size = SDL_strlen( str );
	ASSERT_AND_IF_NOT( size < bufferSize );
	return cmp_write_str( cmp, str, (uint32_t)size );
}

static bool cmpSerializer_writeU32( struct Serializer* s, const char* name, uint32_t* i )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	return cmp_write_u32( cmp, *i );
}

static bool cmpSerializer_writeS8( struct Serializer* s, const char* name, int8_t* c )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( c != NULL ) return false;
	return cmp_write_s8( cmp, *c );
}

static bool cmpSerializer_writeFloat( struct Serializer* s, const char* name, float* f )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( f != NULL ) return false;
	return cmp_write_float( cmp, *f );
}

static bool cmpSerializer_writeUInt( struct Serializer* s, const char* name, uint64_t* u )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( u != NULL ) return false;
	return cmp_write_uinteger( cmp, *u );
}

static bool cmpSerializer_writeS32( struct Serializer* s, const char* name, int32_t* i )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	return cmp_write_s32( cmp, *i );
}

static bool cmpSerializer_writeBool( struct Serializer* s, const char* name, bool* b )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( b != NULL ) return false;
	return cmp_write_bool( cmp, *b );
}

static bool cmpSerializer_writeArraySize( struct Serializer* s, const char* name, uint32_t* size )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( size != NULL ) return false;
	return cmp_write_array( cmp, *size );
}

static bool cmpSerializer_writeTrackedECPSCallback( struct Serializer* s, const char* name, TrackedCallback* c )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( c != NULL ) return false;

	const char* id = ecps_GetTrackedIDFromECPSCallback( *c );
	size_t size = SDL_strlen( id );
	return cmp_write_str( cmp, id, (uint32_t)size );
}

static bool cmpSerializer_writeTrackedEaseFunc( struct Serializer* s, const char* name, EaseFunc* e )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( e != NULL ) return false;

	const char* id = ecps_GetTrackedIDFromTweenFunc( *e );
	size_t size = SDL_strlen( id );
	return cmp_write_str( cmp, id, (uint32_t)size );
}

static bool cmpSerializer_writeImageID( struct Serializer* s, const char* name, ImageID* imgID )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( imgID != NULL ) return false;

	const char* id = img_GetImgStringID( *imgID );
	return cmp_write_str( cmp, id, (uint32_t)SDL_strlen( id ) );
}

static bool cmpSerializer_writeEntityID( struct Serializer* s, const char* name, uint32_t* id )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( id != NULL ) return false;

	uint32_t localID;
	if( !s->entityAccessor.getLocalID( &s->entityAccessor, (EntityID)( *id ), &localID ) ) {
		return false;
	}
	return cmp_write_u32( cmp, localID );
}

void serializer_CreateWriteCmp( void* ctx, Serializer* outSerializer )
{
	ASSERT_AND_IF_NOT( ctx != NULL ) return;
	ASSERT_AND_IF_NOT( outSerializer != NULL ) return;

	*outSerializer = ( Serializer ){
		(void*)ctx,
		cmpSerializer_startWriteStructure,
		cmpSerializer_endWriteStructure,
		cmpSerializer_writeInteger,
		cmpSerializer_writeStr,
		cmpSerializer_writeStrBuffer,
		cmpSerializer_writeU32,
		cmpSerializer_writeS8,
		cmpSerializer_writeFloat,
		cmpSerializer_writeUInt,
		cmpSerializer_writeS32,
		cmpSerializer_writeBool,
		cmpSerializer_writeTrackedECPSCallback,
		cmpSerializer_writeTrackedEaseFunc,
		cmpSerializer_writeImageID,
		cmpSerializer_writeEntityID,
		cmpSerializer_writeArraySize,
		{ NULL, NULL, NULL }
	};

	serializer_SetRawEntityID( outSerializer );
}
#pragma endregion

#pragma region CMP READER
static bool cmpSerializer_startReadStructure( struct Serializer* s, const char* name )
{
	// nothing
	return true;
}

static bool cmpSerializer_endReadStructure( struct Serializer* s, const char* name )
{
	// nothing
	return true;
}


static bool cmpSerializer_readInteger( struct Serializer* s, const char* name, int64_t* d )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( d != NULL ) return false;
	return cmp_read_integer( cmp, d );
}

static bool cmpSerializer_readStr( struct Serializer* s, const char* name, char** str )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( str != NULL ) return false;

	uint32_t size;
	if( !cmp_read_str_size( cmp, &size ) ) {
		return false;
	}

	// got the size, just read the bytes
	char* readStr = mem_Allocate( size + 1 );
	if( !cmp->read( cmp, readStr, size ) ) {
		mem_Release( readStr );
		return false;
	}
	readStr[size] = 0;

	// replace the existing string
	mem_Release( *str );
	*str = readStr;

	return true;
}

static bool cmpSerializer_readStrBuffer( struct Serializer* s, const char* name, char* str, size_t bufferSize )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( str != NULL ) return false;
	ASSERT_AND_IF_NOT( bufferSize <= UINT32_MAX ) return false;
	uint32_t sizeAsU32 = (uint32_t)bufferSize;
	return cmp_read_str( cmp, str, &sizeAsU32 );
}

static bool cmpSerializer_readU32( struct Serializer* s, const char* name, uint32_t* i )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	return cmp_read_u32( cmp, i );
}

static bool cmpSerializer_readS8( struct Serializer* s, const char* name, int8_t * c )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( c != NULL ) return false;
	return cmp_read_s8( cmp, c );
}

static bool cmpSerializer_readFloat( struct Serializer* s, const char* name, float* f )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( f != NULL ) return false;
	return cmp_read_float( cmp, f );
}

static bool cmpSerializer_readUInt( struct Serializer* s, const char* name, uint64_t* u )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( u != NULL ) return false;
	return cmp_read_uinteger( cmp, u );
}

static bool cmpSerializer_readS32( struct Serializer* s, const char* name, int32_t* i )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	return cmp_read_s32( cmp, i );
}

static bool cmpSerializer_readBool( struct Serializer* s, const char* name, bool* b )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( b != NULL );
	return cmp_read_bool( cmp, b );
}

static bool cmpSerializer_readArraySize( struct Serializer* s, const char* name, uint32_t* size )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( size != NULL );
	return cmp_read_array( cmp, size );
}

static bool cmpSerializer_readTrackedECPSCallback( struct Serializer* s, const char* name, TrackedCallback* c )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( c != NULL ) return false;

	char* id = NULL;
	if( !cmpSerializer_readStr( s, name, &id ) ) {
		return false;
	}

	(*c) = ecps_GetTrackedECPSCallbackFromID( id );
	return true;
}

static bool cmpSerializer_readTrackedEaseFunc( struct Serializer* s, const char* name, EaseFunc* e )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( e != NULL ) return false;

	char* id = NULL;
	if( !cmpSerializer_readStr( s, name, &id ) ) {
		return false;
	}

	(*e) = ecps_GetTrackedTweenFuncFromID( id );
	return true;
}

static bool cmpSerializer_readImageID( struct Serializer* s, const char* name, ImageID* imgID )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( imgID != NULL ) return false;


	char* id = NULL;
	if( !cmpSerializer_readStr( s, name, &id ) ) {
		return false;
	}

	(*imgID) = img_GetExistingByStrID( id );
	return true;
}

static bool cmpSerializer_readEntityID( struct Serializer* s, const char* name, uint32_t* id )
{
	CMP_STANDARD_START;
	ASSERT_AND_IF_NOT( id != NULL ) return false;

	uint32_t readVal;
	if( !cmp_read_u32( cmp, &readVal ) ) {
		return false;
	}

	EntityID entityID;
	if( !s->entityAccessor.getEntityID( &s->entityAccessor, readVal, &entityID ) ) {
		return false;
	}
	( *id ) = (uint32_t)entityID;

	return true;
}

void serializer_CreateReadCmp( void* ctx, Serializer* outSerializer )
{
	ASSERT_AND_IF_NOT( ctx != NULL ) return;
	ASSERT_AND_IF_NOT( outSerializer != NULL ) return;

	*outSerializer = (Serializer){
		(void*)ctx,
		cmpSerializer_startReadStructure,
		cmpSerializer_endReadStructure,
		cmpSerializer_readInteger,
		cmpSerializer_readStr,
		cmpSerializer_readStrBuffer,
		cmpSerializer_readU32,
		cmpSerializer_readS8,
		cmpSerializer_readFloat,
		cmpSerializer_readUInt,
		cmpSerializer_readS32,
		cmpSerializer_readBool,
		cmpSerializer_readTrackedECPSCallback,
		cmpSerializer_readTrackedEaseFunc,
		cmpSerializer_readImageID,
		cmpSerializer_readEntityID,
		cmpSerializer_readArraySize,
		{ NULL, NULL, NULL }
	};

	serializer_SetRawEntityID( outSerializer );
}
#pragma endregion

#pragma endregion

//**********************************
// lua serializer
#pragma region LUA SERIALIZERS
#define LUA_STANDARD_START( stackUsed ) \
	ASSERT_AND_IF_NOT( s != NULL ) return false; \
	ASSERT_AND_IF_NOT( s->ctx != NULL ) return false; \
	ASSERT_AND_IF_NOT( name != NULL ) return false; \
	lua_State* ls = (lua_State*)(s->ctx); \
	{ int stackCheck = lua_checkstack( ls, stackUsed ); \
	ASSERT_AND_IF_NOT( stackCheck ) return false; }

#pragma region LUA WRITER
static bool luaSerializer_startWriteStructure( struct Serializer* s, const char* name )
{
	// see if this is an embedded table, if it is then 
	LUA_STANDARD_START( 2 );

	bool embedded = lua_istable( ls, -1 );
	if( embedded ) {
		lua_pushstring( ls, name ); // push key
	}

	lua_newtable( ls );

	return true;
}

static bool luaSerializer_endWriteStructure( struct Serializer* s, const char* name )
{
	LUA_STANDARD_START( 0 );

	bool embedded = lua_istable( ls, -3 );
	if( embedded ) {
		lua_settable( ls, -3 ); // call set table if we're an embedded table
	}
	return true;
}
static bool luaSerializer_writeS64( struct Serializer* s, const char* name, int64_t* d )
{
	ASSERT_AND_IF_NOT( d != NULL ) return false;
	LUA_STANDARD_START( 2 );

	// assuming the table is at the top, sets table[name] = d
	lua_pushstring( ls, name ); // push key
	lua_pushinteger( ls, (lua_Integer)(*d) ); // push value
	lua_settable( ls, -3 ); // call set table

	return true;
}

// for strings we might want to assume we're taking in standard c-strings
static bool luaSerializer_writeStr( struct Serializer* s, const char* name, char** data )
{
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushstring( ls, *data );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeStrBuffer( struct Serializer* s, const char* name, char* data, size_t bufferSize )
{
	ASSERT_AND_IF_NOT( data != NULL ) return false;
	ASSERT_AND_IF_NOT( SDL_strlen( data ) < bufferSize ) return false;

	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushstring( ls, data );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeU32( struct Serializer* s, const char* name, uint32_t* i )
{
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushinteger( ls, (lua_Integer)(*i) );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeS8( struct Serializer* s, const char* name, int8_t* c )
{
	ASSERT_AND_IF_NOT( c != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushinteger( ls, (lua_Integer)(*c) );
	lua_settable( ls, -3 );

	return false;
}

static bool luaSerializer_writeFloat( struct Serializer* s, const char* name, float* f )
{
	ASSERT_AND_IF_NOT( f != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushnumber( ls, (lua_Number)(*f) );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeU64( struct Serializer* s, const char* name, uint64_t* u )
{
	ASSERT_ALWAYS( "Unsigned 64-bit integer type not supported by Lua, uses a signed 64-bit integer." );
	return false;
}

static bool luaSerializer_writeS32( struct Serializer* s, const char* name, int32_t* i )
{
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushinteger( ls, (lua_Integer)(*i) );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeBool( struct Serializer* s, const char* name, bool* b )
{
	ASSERT_AND_IF_NOT( b != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushboolean( ls, XLUA_TO_BOOL( *b ) );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeArraySize( struct Serializer* s, const char* name, uint32_t* size )
{
	ASSERT_AND_IF_NOT( size != NULL ) return false;
	LUA_STANDARD_START( 2 );

	lua_pushstring( ls, name );
	lua_pushinteger( ls, (lua_Integer)(*size) );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeTrackedECPSCallback( struct Serializer* s, const char* name, TrackedCallback* c )
{
	LUA_STANDARD_START( 2 );
	ASSERT_AND_IF_NOT( c != NULL ) return false;

	const char* id = ecps_GetTrackedIDFromECPSCallback( *c );

	lua_pushstring( ls, name );
	lua_pushstring( ls, id );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeTrackedEaseFunc( struct Serializer* s, const char* name, EaseFunc* e )
{
	LUA_STANDARD_START( 2 );
	ASSERT_AND_IF_NOT( e != NULL ) return false;

	const char* id = ecps_GetTrackedIDFromTweenFunc( *e );

	lua_pushstring( ls, name );
	lua_pushstring( ls, id );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeImageID( struct Serializer* s, const char* name, ImageID* imgID )
{
	LUA_STANDARD_START( 2 );
	ASSERT_AND_IF_NOT( imgID != NULL ) return false;

	lua_pushstring( ls, name );
	lua_pushinteger( ls, *imgID );
	lua_settable( ls, -3 );

	return true;
}

static bool luaSerializer_writeEntityID( struct Serializer* s, const char* name, uint32_t* id )
{
	ASSERT_AND_IF_NOT( id != NULL ) return false;

	EntityID eid = (EntityID)*id;
	uint32_t localID;
	if( !s->entityAccessor.getLocalID( &s->entityAccessor, eid, &localID ) ) {
		return false;
	}

	luaSerializer_writeU32( s, name, &localID );

	return true;
}

void serializer_CreateWriteLua( void* ctx, Serializer* outSerializer )
{
	ASSERT_AND_IF_NOT( ctx != NULL ) return;
	ASSERT_AND_IF_NOT( outSerializer != NULL ) return;

	*outSerializer = ( Serializer ){
		(void*)ctx,
		luaSerializer_startWriteStructure,
		luaSerializer_endWriteStructure,
		luaSerializer_writeS64,
		luaSerializer_writeStr,
		luaSerializer_writeStrBuffer,
		luaSerializer_writeU32,
		luaSerializer_writeS8,
		luaSerializer_writeFloat,
		luaSerializer_writeU64,
		luaSerializer_writeS32,
		luaSerializer_writeBool,
		luaSerializer_writeTrackedECPSCallback,
		luaSerializer_writeTrackedEaseFunc,
		luaSerializer_writeImageID,
		luaSerializer_writeEntityID,
		luaSerializer_writeArraySize,
		{ NULL, NULL, NULL }
	};

	serializer_SetRawEntityID( outSerializer );
}

#pragma endregion

#pragma region LUA READER

// pushes k[entryName] onto the top of the stack
#define STANDARD_READ_START( luaState, entryName ) if( !getTableEntry( (luaState), (entryName) ) ) return false;

// pops off the value pushed on from STANDARD_READ_START
#define STANDARD_READ_END( luaState ) lua_pop( (luaState), 1 );

// gets the table entry onto the top of the stack
static bool getTableEntry( lua_State* ls, const char* entryName )
{
	lua_pushstring( ls, entryName );
	int ret = lua_gettable( ls, -2 ); // get table[name], pops the top as the name and pushes the value it onto the stack
	ASSERT( ( ret != 0 ) && "Key doesn't exist in table" );
	return ret;
}

static bool luaSerializer_startReadStructure( struct Serializer* s, const char* name )
{
	LUA_STANDARD_START( 3 );

	// make sure top most thing is a table
	ASSERT_AND_IF_NOT( lua_istable( ls, -1 ) ) return false;

	if( lua_gettop( ls ) == 1 ) {
		// there is only the initial table on the stack
		lua_pushnil( ls );
		lua_insert( ls, 1 ); // move the nil to the bottom of the stack, this is a marker used to indicate that we already started
	} else {
		// the marker value has been pushed onto the stack
		STANDARD_READ_START( ls, name );
	}

	return true;
}

static bool luaSerializer_endReadStructure( struct Serializer* s, const char* name )
{
	LUA_STANDARD_START( 0 );

	if( lua_gettop( ls ) == 2 ) {
		// only the table and marker nil should be on the stack
		ASSERT_AND_IF_NOT( lua_istable( ls, -1 ) ) return false;
		ASSERT_AND_IF_NOT( lua_isnil( ls, -2 ) ) return false;

		// remove the marker
		lua_remove( ls, 1 );
	} else {
		// pop the embedded table off so we can access the base table again
		STANDARD_READ_END( ls );
	}
	
	return true;
}

static bool luaSerializer_readS64( struct Serializer* s, const char* name, int64_t* d )
{
	ASSERT_AND_IF_NOT( d != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );

	bool ret = false;

	int isType;
	lua_Integer result = lua_tointegerx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	(*d) = (int64_t)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readStr( struct Serializer* s, const char* name, char** data )
{
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );

	bool ret = false;
	if( !lua_isstring( ls, -1 ) ) goto clean_up;

	const char* result = lua_tostring( ls, -1 );
	ASSERT_AND_IF_NOT( result != NULL ) return false;
	size_t strSize = (size_t)lua_rawlen( ls, -1 );

	// create a local copy of the string
	mem_Release( *data );
	*data = createStringCopy( result );
	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readStrBuffer( struct Serializer* s, const char* name, char* data, size_t bufferSize )
{
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );

	bool ret = false;
	if( !lua_isstring( ls, -1 ) ) goto clean_up;

	const char* result = lua_tostring( ls, -1 );
	ASSERT_AND_IF_NOT( result != NULL ) return false;
	size_t strSize = (size_t)lua_rawlen( ls, -1 );

	ASSERT_AND_IF_NOT( strSize < bufferSize ) return false;

	// copy the string over the buffer
	SDL_memcpy( data, result, strSize );
	data[strSize] = 0;
	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}


static bool luaSerializer_readU32( struct Serializer* s, const char* name, uint32_t* i )
{
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	int isType;
	lua_Integer result = lua_tointegerx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	ASSERT_AND_IF_NOT( ( result >= 0 ) && ( result <= UINT32_MAX ) ) goto clean_up;
	(*i) = (uint32_t)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readS8( struct Serializer* s, const char* name, int8_t* c ) // cmp_read_s8
{
	ASSERT_AND_IF_NOT( c != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	int isType;
	lua_Integer result = lua_tointegerx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	ASSERT_AND_IF_NOT( ( result >= INT8_MIN ) && ( result <= INT8_MAX ) ) goto clean_up;
	(*c) = (int8_t)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readFloat( struct Serializer* s, const char* name, float* f ) // cmp_read_float
{
	ASSERT_AND_IF_NOT( f != NULL ) return false;
	LUA_STANDARD_START( 2 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	int isType;	
	lua_Number result = lua_tonumberx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	ASSERT_AND_IF_NOT( ( result >= -FLT_MAX ) && ( result <= FLT_MAX ) ) goto clean_up;
	(*f) = (float)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readU64( struct Serializer* s, const char* name, uint64_t* u ) // cmp_read_uinteger
{
	ASSERT_ALWAYS( "Unsigned 64-bit integer type not supported by Lua, uses a signed 64-bit integer." );
	return false;
}

static bool luaSerializer_readS32( struct Serializer* s, const char* name, int32_t* i ) // cmp_read_s32
{
	ASSERT_AND_IF_NOT( i != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	int isType;
	lua_Integer result = lua_tointegerx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	ASSERT_AND_IF_NOT( ( result >= INT32_MIN ) && ( result <= INT32_MAX ) ) goto clean_up;
	(*i) = (int32_t)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readBool( struct Serializer* s, const char* name, bool* b ) // cmp_read_bool
{
	ASSERT_AND_IF_NOT( b != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	ASSERT_AND_IF_NOT( lua_isboolean( ls, -1 ) ) goto clean_up;
	int result = lua_toboolean( ls, -1 );
	(*b) = XLUA_TO_BOOL( result );

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readArraySize( struct Serializer* s, const char* name, uint32_t* size )
{
	return luaSerializer_readU32( s, name, size );
}

static bool luaSerializer_readTrackedECPSCallback( struct Serializer* s, const char* name, TrackedCallback* c )
{
	ASSERT_AND_IF_NOT( c != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );

	bool ret = false;
	if( !lua_isstring( ls, -1 ) ) goto clean_up;

	const char* id = lua_tostring( ls, -1 );
	(*c) = ecps_GetTrackedECPSCallbackFromID( id );
	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readTrackedEaseFunc( struct Serializer* s, const char* name, EaseFunc* e )
{
	ASSERT_AND_IF_NOT( e != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );

	bool ret = false;
	if( !lua_isstring( ls, -1 ) ) goto clean_up;

	const char* id = lua_tostring( ls, -1 );
	(*e) = ecps_GetTrackedTweenFuncFromID( id );
	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readImageID( struct Serializer* s, const char* name, ImageID* imgID )
{
	ASSERT_AND_IF_NOT( imgID != NULL ) return false;
	LUA_STANDARD_START( 1 );
	STANDARD_READ_START( ls, name );
	bool ret = false;

	int isType;
	lua_Integer result = lua_tointegerx( ls, -1, &isType );
	ASSERT_AND_IF_NOT( isType ) goto clean_up;
	( *imgID ) = (ImageID)result;

	ret = true;

clean_up:
	STANDARD_READ_END( ls );
	return ret;
}

static bool luaSerializer_readEntityID( struct Serializer* s, const char* name, uint32_t* id )
{
	ASSERT_AND_IF_NOT( id != NULL ) return false;

	uint32_t readID;
	if( !luaSerializer_readU32( s, name, &readID ) ) {
		return false;
	}

	EntityID eid;
	if( !s->entityAccessor.getEntityID( &s->entityAccessor, readID, &eid ) ) {
		return false;
	}

	( *id ) = (uint32_t)eid;
	return true;
}

void serializer_CreateReadLua( void* ctx, Serializer* outSerializer )
{
	ASSERT_AND_IF_NOT( ctx != NULL ) return;
	ASSERT_AND_IF_NOT( outSerializer != NULL ) return;

	*outSerializer = ( Serializer ){
		(void*)ctx,
		luaSerializer_startReadStructure,
		luaSerializer_endReadStructure,
		luaSerializer_readS64,
		luaSerializer_readStr,
		luaSerializer_readStrBuffer,
		luaSerializer_readU32,
		luaSerializer_readS8,
		luaSerializer_readFloat,
		luaSerializer_readU64,
		luaSerializer_readS32,
		luaSerializer_readBool,
		luaSerializer_readTrackedECPSCallback,
		luaSerializer_readTrackedEaseFunc,
		luaSerializer_readImageID,
		luaSerializer_readEntityID,
		luaSerializer_readArraySize,
		{ NULL, NULL, NULL }
	};

	serializer_SetRawEntityID( outSerializer );
}
#pragma endregion

static bool rawGetEntityID( struct EntityAccessor* a, uint32_t localID, EntityID* outID )
{
	ASSERT_AND_IF_NOT( outID != NULL ) return false;
	( *outID ) = (EntityID)localID;
	return true;
}

static bool rawGetLocalID( struct EntityAccessor* a, EntityID id, uint32_t* outID )
{
	ASSERT_AND_IF_NOT( outID != NULL ) return false;
	( *outID ) = (uint32_t)id;
	return true;
}

void serializer_SetRawEntityID( Serializer* serializer )
{
	ASSERT_AND_IF_NOT( serializer != NULL ) return;

	serializer->entityAccessor.ctx = NULL;
	serializer->entityAccessor.getEntityID = rawGetEntityID;
	serializer->entityAccessor.getLocalID = rawGetLocalID;
}

//---

static bool entityInfoGetEntityID( struct EntityAccessor* a, uint32_t localID, EntityID* outID )
{
	ASSERT_AND_IF_NOT( a != NULL ) return false;
	ASSERT_AND_IF_NOT( a->ctx != NULL ) return false;
	ASSERT_AND_IF_NOT( outID != NULL ) return false;

	SerializedEntityInfo* sei = (SerializedEntityInfo*)( a->ctx );
	(*outID) = ecps_GetEntityIDfromSerializedLocalID( sei, localID );
	return true;
}

static bool entityInfoGetLocalID( struct EntityAccessor* a, EntityID id, uint32_t* outID )
{
	ASSERT_AND_IF_NOT( a != NULL ) return false;
	ASSERT_AND_IF_NOT( a->ctx != NULL ) return false;
	ASSERT_AND_IF_NOT( outID != NULL ) return false;

	SerializedEntityInfo* sei = (SerializedEntityInfo*)( a->ctx );
	(*outID) = ecps_GetLocalIDFromSerializeEntityID( sei, id );
	return true;
}


void serializer_SetEntityInfo( void* ctx, Serializer* serializer )
{
	ASSERT_AND_IF_NOT( ctx != NULL ) return;
	ASSERT_AND_IF_NOT( serializer != NULL ) return;

	serializer->entityAccessor.ctx = ctx;
	serializer->entityAccessor.getEntityID = entityInfoGetEntityID;
	serializer->entityAccessor.getLocalID = entityInfoGetLocalID;
}

#pragma endregion