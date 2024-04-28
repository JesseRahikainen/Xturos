#include "luaInterface.h"

#include <SDL.h>
#include <varargs.h>

#include "System/platformLog.h"
#include "System/memory.h"
#include "Utils/stretchyBuffer.h"

static lua_State* luaState = NULL;

//************************************************************************************
// C level callbacks
static void* memoryAllocation( void* ud, void* ptr, size_t oldSize, size_t newSize )
{
	// based on this:
	//  https://www.lua.org/manual/5.3/manual.html#4.8
	if( newSize == 0 ) {
		mem_Release( ptr );
		return NULL;
	}

	return mem_Resize( ptr, newSize );
}

static void warnFunction( void* ud, const char* msg, int tocont )
{
	llog( LOG_WARN, msg );
}

static int errorFunction( lua_State* ls )
{
	// pushes the traceback onto the stack to be handled in the function that called whatever had an error
	const char* msg = lua_tostring( ls, -1 );
	luaL_traceback( ls, ls, msg, 1 );
	lua_remove( ls, -2 );
	return 1;
}


//************************************************************************************
// Helper functions

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
bool xLua_GetNextInt( lua_State* ls, int* p, int* out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( !lua_isinteger( ls, *p ) ) {
		// error
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = (int)lua_tointeger( ls, *p );
		--( *p );
		return true;
	}
}

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
bool xLua_GetNextString( lua_State* ls, int* p, const char** out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( !lua_isstring( ls, *p ) ) {
		// error
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = lua_tostring( ls, *p );
		--( *p );
		return true;
	}
}

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
bool xLua_GetNextFloat( lua_State* ls, int* p, float* out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( !lua_isnumber( ls, *p ) ) {
		// error
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = (float)lua_tonumber( ls, *p );
		--( *p );
		return true;
	}
}


int xLua_DoCall( int narg, int nres )
{
	int status;
	int base = lua_gettop( luaState ) - narg;  /* function index */
	lua_pushcfunction( luaState, errorFunction );  /* push message handler */
	lua_insert( luaState, base );  /* put it under function and args */
	status = lua_pcall( luaState, narg, nres, base );
	lua_remove( luaState, base );  /* remove message handler from the stack */
	return status;
}

bool xLua_GetGlobalFunc( const char* funcName )
{
	int type = lua_getglobal( luaState, funcName );
	if( type != LUA_TFUNCTION ) {
		llog( LOG_ERROR, "%s is not a global function", funcName );
		return false;
	}
	return true;
}

void xLua_RegisterCFunction( const char* luaFuncName, lua_CFunction cFunc )
{
	SDL_assert( luaState != NULL );
	// todo: error handling
	lua_register( luaState, luaFuncName, cFunc );
}

// the signature is a string we use to define the parameters to send into the function
//  and the returns to get back from it
// i = integer
// d = double
// f = float
// s = string
// b = bool
// n = nil when parameter, ignored when return, do not pass in anything for nil parameters
// | = break between parameters and return
// so the signature "df|si" would be for a function that takes a double and a float, and returns a string and an integer
//  based on code from here: https://www.lua.org/pil/25.3.html
// for strings we create a copy of it, so they must be released after being returned from here, use mem_Release
//  if the string is empty or returns NULL we just return NULL for the value
bool xLua_CallLuaFunction( const char* funcName, const char* signature, ... )
{
	int argCnt = 0;
	int retCnt = 0;

	if( !xLua_GetGlobalFunc( funcName ) ) {
		return false;
	}

	va_list vl;
	va_start( vl, signature );
	bool paramsDone = false;
	while( ( signature != NULL ) && ( *signature ) && !paramsDone ) {
		switch( *signature ) {
		case 'i':
			lua_pushinteger( luaState, va_arg( vl, int ) );
			break;
		case 'd':
			lua_pushnumber( luaState, va_arg( vl, double ) );
			break;
		case 'f':
			lua_pushnumber( luaState, va_arg( vl, float ) );
			break;
		case 's':
			lua_pushstring( luaState, va_arg( vl, char* ) );
			break;
		case 'b':
			lua_pushboolean( luaState, va_arg( vl, bool ) ? 1 : 0 );
			break;
		case 'n':
			lua_pushnil( luaState );
			break;
		case '|':
			paramsDone = true;
			break;
		default:
			llog( LOG_ERROR, "Unknown parameter definition '%c' when calling function %s.", *signature, funcName );
			break;
		}
		++signature;

		if( !paramsDone ) {
			++argCnt;
			if( !lua_checkstack( luaState, 1 ) ) {
				llog( LOG_ERROR, "Too many parameters to Lua function." );
				return false;
			}
		}
	}

	// call the function
	retCnt = (int)SDL_strlen( signature );
	int status = xLua_DoCall( argCnt, retCnt );
	if( status != LUA_OK ) {
		// there was an error, none of the return signature values are needed as there won't be any
		//  just log the error and return that it failed
		llog( LOG_ERROR, "%s", lua_tostring( luaState, -1 ) );
		lua_pop( luaState, 1 );
		return false;
	}

	int idx = -retCnt;
	while( ( signature != NULL ) && *signature ) {
		switch( *signature ) {
		case 'i':
			*va_arg( vl, int* ) = (int)lua_tointeger( luaState, idx );
			break;
		case 'd':
			*va_arg( vl, double* ) = lua_tonumber( luaState, idx );
			break;
		case 'f':
			*va_arg( vl, float* ) = (float)lua_tonumber( luaState, idx );
			break;
		case 's': {
			size_t size;
			const char* luaStr = lua_tolstring( luaState, idx, &size );
			char* retStr = va_arg( vl, char* );
			if( ( luaStr == NULL ) || ( size == 0 ) ) {
				retStr = NULL;
			} else {
				retStr = mem_Allocate( sizeof( char ) * ( size + 1 ) );
				SDL_memcpy( retStr, luaStr, sizeof( char ) * ( size + 1 ) );
			}
		}
			break;
		case 'b':
			*va_arg( vl, bool* ) = ( lua_toboolean( luaState, idx ) == 1 );
			break;
		case 'n':
			// don't do anything
			break;
		default:
			llog( LOG_ERROR, "Unknown parameter definition '%c' when calling function %s.", *signature, funcName );
			break;
		}

		++idx;
		++signature;
	}
	lua_pop( luaState, retCnt );

	va_end( vl );

	return true;
}

// gets all the keys in a global table
//  uses stretchy buffers, so use the sb_ functions to query and clean up
char** xLua_GetLuaTableKeys( const char* tableName )
{
	char** ret = NULL;

	//llog( LOG_DEBUG, "=== getting table: %s", tableName );
	// based on this: https://stackoverflow.com/questions/6137684/iterate-through-lua-table
	lua_State* ls = xLua_GetState( );
	lua_getglobal( ls, tableName );
	if( !lua_istable( ls, -1 ) ) {
		llog( LOG_ERROR, "Unable to get table %s", tableName );
		return NULL;
	}

	// we just want the keys for this table
	// stack is now: -1 = table
	lua_pushnil( ls ); // set current key to nil which tells Lua to start at the beginning
	// stack is now -1 = nil, -2 = table
	while( lua_next( ls, -2 ) != 0 ) {
		// stack is now -1 = value, -2 = key, -3 = table
		lua_pushvalue( ls, -2 ); // copy the key so that lua_tostring doesn't modify the original
		// stack is now -1 = key, -2 = value, -3 = key, -4 = table
		const char* key = lua_tostring( ls, -1 );

		// create a copy of the string and push it onto the list
		size_t len = SDL_strlen( key ) + 1;
		char* str = NULL;
		sb_Add( str, len );
		SDL_strlcpy( str, key, len );
		sb_Push( ret, str );

		lua_pop( ls, 2 ); // popvalue + copy of the key, leaving original key for lua_next( )
		// stack is now -1 = key, -2 = table
	}

	// stack is now: -1 = table
	lua_pop( ls, 1 );

	return ret;
}

// convience clean up function, sbKeys will be invalid after this
void xLua_CleanUpTableKeys( char** sbKeys )
{
	for( size_t i = 0; i < sb_Count( sbKeys ); ++i ) {
		sb_Release( sbKeys[i] );
	}
	sb_Release( sbKeys );
}

//************************************************************************************
// Main functions
bool xLua_LoadAndDoFile( const char* fileName )
{
	SDL_assert( luaState != NULL );
	
	if( luaState == NULL ) {
		llog( LOG_ERROR, "Lua not initialized." );
		return false;
	}

	if( ( fileName == NULL ) || SDL_strlen( fileName ) <= 0 ) {
		llog( LOG_ERROR, "Attempting to load file with no name." );
		return false;
	}

	int status = luaL_dofile( luaState, fileName );
	if( status != LUA_OK ) {
		llog( LOG_ERROR, "0x%x error while loading file %s: %s", status, fileName, lua_tostring( luaState, -1 ) );
		lua_pop( luaState, 1 );
		return false;
	}
	return true;
}

// pulled from linit.c
static const luaL_Reg loadedlibs[] = {
	{ LUA_GNAME, luaopen_base },
	{ LUA_LOADLIBNAME, luaopen_package },
	{ LUA_COLIBNAME, luaopen_coroutine },
	{ LUA_TABLIBNAME, luaopen_table },
	//{ LUA_IOLIBNAME, luaopen_io }, // don't want random scripts to be able to access io functions
	//{ LUA_OSLIBNAME, luaopen_os }, // don't want random scripts to be able to access os functions
	{ LUA_STRLIBNAME, luaopen_string },
	{ LUA_MATHLIBNAME, luaopen_math },
	{ LUA_UTF8LIBNAME, luaopen_utf8 },
#ifdef _DEBUG
	{ LUA_DBLIBNAME, luaopen_debug },
#endif
	{ NULL, NULL }
};

bool xLua_Init( void )
{
	if( luaState != NULL ) {
		llog( LOG_WARN, "Attempting to initialize Lua a second time." );
		return false;
	}

	luaState = lua_newstate( memoryAllocation, NULL );
	if( luaState == NULL ) {
		return false;
	}

	lua_setwarnf( luaState, warnFunction, NULL );

	// load the basic libraries, copied from linit.c but modified to only open the libraries we want
	const luaL_Reg* lib;
	for( lib = loadedlibs; lib->func; ++lib ) {
		luaL_requiref( luaState, lib->name, lib->func, 1 );
		lua_pop( luaState, 1 );  /* remove lib */
	}

	return true;
}

void xLua_ShutDown( void )
{
	if( luaState == NULL ) {
		llog( LOG_WARN, "Attempting to shut down Lua when it was never initialized." );
		return;
	}

	lua_close( luaState );
	luaState = NULL;
}

// for when you want to access it for something the interface doesn't provide
lua_State* xLua_GetState( void )
{
	return luaState;
}

// just dump the stack out to the log
void xLua_StackDump( void )
{
	int i;
	int top = lua_gettop( luaState );
	llog( LOG_DEBUG, "=== Start stack dump" );
	for( i = 1; i <= top; i++ ) {  /* repeat for each level */
		int t = lua_type( luaState, i );
		switch( t ) {

		case LUA_TSTRING:  /* strings */
			llog( LOG_DEBUG, "`%s'", lua_tostring( luaState, i ) );
			break;

		case LUA_TBOOLEAN:  /* booleans */
			llog( LOG_DEBUG, lua_toboolean( luaState, i ) ? "true" : "false" );
			break;

		case LUA_TNUMBER:  /* numbers */
			llog( LOG_DEBUG, "%g", lua_tonumber( luaState, i ) );
			break;

		default:  /* other values */
			llog( LOG_DEBUG, "%s", lua_typename( luaState, t ) );
			break;

		}
	}

	llog( LOG_DEBUG, "End stack dump ===" );
}