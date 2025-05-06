#ifndef LUA_INTERFACE_H
#define LUA_INTERFACE_H

#include <stdbool.h>

#if SCRIPTING_ENABLED

#include "Others/lua-5.4.3/lua.h"
#include "Others/lua-5.4.3/lualib.h"
#include "Others/lua-5.4.3/lauxlib.h"

// Lua uses ints for bools, lets us convert between the two more explicitly
// convert Lua bool (int based) to bool
#define XLUA_TO_BOOL( i ) ( i != 0 )
// convert bool to Lua bool (int based)
#define XLUA_FROM_BOOL( b ) ( b ? 1 : 0 )

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
bool xLua_GetNextInt( lua_State* ls, int* p, int* out, int* errorOut, const char* name );
bool xLua_GetNextString( lua_State* ls, int* p, const char** out, int* errorOut, const char* name );
bool xLua_GetNextFloat( lua_State* ls, int* p, float* out, int* errorOut, const char* name );
bool xLua_GetNextBool( lua_State* ls, int* p, bool* out, int* errorOut, const char* name );

int xLua_DoCall( int narg, int nres );
bool xLua_GetGlobalFunc( const char* funcName );
void xLua_RegisterCFunction( const char* luaFuncName, lua_CFunction cFunc );

bool xLua_CallLuaFunction( const char* funcName, const char* paramDef, ... );

// gets all the keys in a global table
//  uses stretchy buffers, so use the sb_ functions to query and clean up
char** xLua_GetLuaTableKeys( const char* tableName );

// convience clean up function, sbKeys will be invalid after this
void xLua_CleanUpTableKeys( char** sbKeys );

// loads and executes a file
bool xLua_LoadAndDoFile( const char* fileName );

// initialize Lua
bool xLua_Init( void );

// shutdown Lua
void xLua_ShutDown( void );

// for when you want to access it for something the interface doesn't provide
lua_State* xLua_GetState( void );

// just dump the stack out to the log
void xLua_StackDump( void );

// helpers for retrieving variables when a lua is calling a c function
#define LUA_GET_INT(ls, var, p, error) int var = 0; xLua_GetNextInt( ls, &p, &(var), &error, #var );
#define LUA_GET_FLOAT(ls, var, p, error) float var = 0.0f; xLua_GetNextFloat( ls, &p, &(var), &error, #var );
#define LUA_GET_STRING(ls, var, p, error) const char* var = NULL; xLua_GetNextString( ls, &p, &(var), &error, #var );
#define LUA_GET_BOOL(ls, var, p, error) bool var = false; xLua_GetNextBool( ls, &p, &(var), &error, #var );

#else

bool xLua_Init( void );
void xLua_ShutDown( void );
bool xLua_LoadAndDoFile( const char* fileName );
bool xLua_CallLuaFunction( const char* funcName, const char* paramDef, ... );

#endif // scripting enable

#endif // inclusion guard