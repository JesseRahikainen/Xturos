#ifndef LUA_INTERFACE_H
#define LUA_INTERFACE_H

#include <stdbool.h>
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

#endif // inclusion guard