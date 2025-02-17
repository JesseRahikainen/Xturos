#include "luaInterface.h"

#include <SDL3/SDL.h>
#include <varargs.h>

#include "System/platformLog.h"
#include "System/memory.h"
#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"

static lua_State* luaState = NULL;

//************************************************************************************
// Some general functions Lua scripts can use.

#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/graphics.h"
#include "sound.h"
#include "Input/input.h"
#include "Graphics/imageSheets.h"
#include "UI/uiEntities.h"
#include "DefaultECPS/defaultECPS.h"

static int lua_LoadImage( lua_State* ls )
{
	// file name
	int p = -1;
	int error;

	LUA_GET_STRING( ls, fileName, p, error );

	int img = img_Load( fileName, ST_DEFAULT );

	lua_pushinteger( ls, img );
	return 1;
}

static int lua_CreateSprite( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, depth, p, error );
	LUA_GET_INT( ls, camFlags, p, error );
	LUA_GET_FLOAT( ls, yPos, p, error );
	LUA_GET_FLOAT( ls, xPos, p, error );
	LUA_GET_INT( ls, imageID, p, error );

	if( depth < INT8_MIN ) {
		llog( LOG_WARN, "Depth too low, clamping to minimum value." );
		depth = INT8_MIN;
	}

	if( depth > INT8_MAX ) {
		llog( LOG_WARN, "Depth to high, clamping to maximum value." );
		depth = INT8_MAX;
	}

	GCTransformData tfData = gc_CreateTransformPos( vec2( xPos, yPos ) );

	GCSpriteData spriteData;
	spriteData.camFlags = camFlags;
	spriteData.depth = (int8_t)depth;
	spriteData.img = imageID;

	EntityID sprite = ecps_CreateEntity( &defaultECPS, 2,
		gcTransformCompID, &tfData,
		gcSpriteCompID, &spriteData );

	lua_pushinteger( ls, (lua_Integer)sprite );
	return 1;
}

static int lua_SetCamFlags( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, flags, p, error );
	LUA_GET_INT( ls, camID, p, error );

	cam_SetFlags( camID, flags );
	return 0;
}

static int lua_SetClearColor( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_FLOAT( ls, alpha, p, error );
	LUA_GET_FLOAT( ls, blue, p, error );
	LUA_GET_FLOAT( ls, green, p, error );
	LUA_GET_FLOAT( ls, red, p, error );

	gfx_SetClearColor( clr( red, green, blue, alpha ) );
	return 0;
}

static int lua_LoadStreamingSound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, group, p, error );
	LUA_GET_STRING( ls, loops, p, error );
	LUA_GET_STRING( ls, fileName, p, error );

	int strmID = snd_LoadStreaming( fileName, loops, (unsigned int)group );

	lua_pushinteger( ls, strmID );
	return 1;
}

static int lua_PlayStreamingSound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_FLOAT( ls, pan, p, error );
	LUA_GET_FLOAT( ls, volume, p, error );
	LUA_GET_INT( ls, streamID, p, error );

	if( ( volume < 0.0f ) || ( volume > 1.0f ) ) {
		llog( LOG_WARN, "Volume must be between 0 and 1, clamping to that range." );
		volume = clamp01( volume );
	}

	if( ( pan < -1.0f ) || ( pan > 1.0f ) ) {
		llog( LOG_WARN, "Panning must be between -1 and 1, clamping to that range." );
		pan = clamp( -1.0f, 1.0f, pan );
	}

	snd_PlayStreaming( streamID, volume, pan, 0 );

	return 0;
}

static int lua_UnloadStreamingSound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, streamID, p, error );

	snd_UnloadStream( streamID );

	return 0;
}

static int lua_LoadSound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, loops, p, error );
	LUA_GET_INT( ls, desiredChannels, p, error );
	LUA_GET_STRING( ls, fileName, p, error );

	if( desiredChannels <= 0 ) {
		llog( LOG_WARN, "Desired channels must be >= 1. Setting to 1." );
		desiredChannels = 1;
	}

	if( desiredChannels > SDL_MAX_UINT8 ) {
		llog( LOG_WARN, "Desired channels must be <= %i. Setting to %i.", SDL_MAX_UINT8, SDL_MAX_UINT8 );
		desiredChannels = SDL_MAX_UINT8;
	}

	int sampleID = snd_LoadSample( fileName, (Uint8)desiredChannels, loops );

	lua_pushinteger( ls, sampleID );
	return 1;
}

static int lua_PlaySound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, group, p, error );
	LUA_GET_FLOAT( ls, pan, p, error );
	LUA_GET_FLOAT( ls, pitch, p, error );
	LUA_GET_FLOAT( ls, volume, p, error );
	LUA_GET_INT( ls, sampleID, p, error );

	if( ( volume < 0.0f ) || ( volume > 1.0f ) ) {
		llog( LOG_WARN, "Volume must be between 0 and 1, clamping to that range." );
		volume = clamp01( volume );
	}

	if( ( pan < -1.0f ) || ( pan > 1.0f ) ) {
		llog( LOG_WARN, "Panning must be between -1 and 1, clamping to that range." );
		pan = clamp( -1.0f, 1.0f, pan );
	}

	EntityID sndID = snd_Play( sampleID, volume, pitch, pan, (unsigned int)group );

	lua_pushinteger( ls, sndID );
	return 1;
}

static int lua_UnloadSound( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, sampleID, p, error );

	snd_UnloadSample( sampleID );

	return 0;
}

static int lua_LoadSpriteSheet( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, fileName, p, error );

	int packageID = img_LoadSpriteSheet( fileName, ST_DEFAULT, NULL );

	lua_pushinteger( ls, packageID );
	return 1;
}

static int lua_CreateButton( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, releaseFuncName, p, error );
	LUA_GET_STRING( ls, pressFuncName, p, error );
	LUA_GET_INT( ls, depth, p, error );
	LUA_GET_INT( ls, camFlags, p, error );
	LUA_GET_FLOAT( ls, backgroundAlpha, p, error );
	LUA_GET_FLOAT( ls, backgroundBlue, p, error );
	LUA_GET_FLOAT( ls, backgroundGreen, p, error );
	LUA_GET_FLOAT( ls, backgroundRed, p, error );
	LUA_GET_INT( ls, background3x3PackageID, p, error );
	LUA_GET_FLOAT( ls, yTextOffset, p, error );
	LUA_GET_FLOAT( ls, xTextOffset, p, error );
	LUA_GET_FLOAT( ls, fontAlpha, p, error );
	LUA_GET_FLOAT( ls, fontBlue, p, error );
	LUA_GET_FLOAT( ls, fontGreen, p, error );
	LUA_GET_FLOAT( ls, fontRed, p, error );
	LUA_GET_FLOAT( ls, textSize, p, error );
	LUA_GET_INT( ls, font, p, error );
	LUA_GET_STRING( ls, text, p, error );
	LUA_GET_FLOAT( ls, height, p, error );
	LUA_GET_FLOAT( ls, width, p, error );
	LUA_GET_FLOAT( ls, yPos, p, error );
	LUA_GET_FLOAT( ls, xPos, p, error );

	int* images = img_GetPackageImages( background3x3PackageID );
	SDL_assert( images != NULL );
	SDL_assert( sb_Count( images ) == 9 );

	if( depth < INT8_MIN ) {
		llog( LOG_WARN, "Depth too low, clamping to minimum value." );
		depth = INT8_MIN;
	}

	if( depth > INT8_MAX ) {
		llog( LOG_WARN, "Depth too high, clamping to minimum value." );
		depth = INT8_MAX;
	}

	EntityID buttonID = button_Create3x3ScriptButton( &defaultECPS, vec2( xPos, yPos ), vec2( width, height ), text, font, textSize, clr( fontRed, fontGreen, fontBlue, fontAlpha ),
		vec2( xTextOffset, yTextOffset ), images, clr( backgroundRed, backgroundGreen, backgroundBlue, backgroundAlpha ), camFlags, (int8_t)depth, pressFuncName, releaseFuncName );

	lua_pushinteger( ls, (lua_Integer)buttonID );
	return 1;
}

static int lua_CreateLabel( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, depth, p, error );
	LUA_GET_INT( ls, camFlags, p, error );
	LUA_GET_FLOAT( ls, textSize, p, error );
	LUA_GET_INT( ls, font, p, error );
	LUA_GET_INT( ls, vAlign, p, error );
	LUA_GET_INT( ls, hAlign, p, error );
	LUA_GET_FLOAT( ls, alpha, p, error );
	LUA_GET_FLOAT( ls, blue, p, error );
	LUA_GET_FLOAT( ls, green, p, error );
	LUA_GET_FLOAT( ls, red, p, error );
	LUA_GET_FLOAT( ls, yPos, p, error );
	LUA_GET_FLOAT( ls, xPos, p, error );
	LUA_GET_STRING( ls, text, p, error );

	if( depth < INT8_MIN ) {
		llog( LOG_WARN, "Depth too low, clamping to minimum value." );
		depth = INT8_MIN;
	}

	if( depth > INT8_MAX ) {
		llog( LOG_WARN, "Depth to high, clamping to maximum value." );
		depth = INT8_MAX;
	}

	EntityID labelID = createLabel( &defaultECPS, text, vec2( xPos, yPos ), clr( red, green, blue, alpha ), hAlign, vAlign, font, textSize, camFlags, (int8_t)depth );

	lua_pushinteger( ls, (lua_Integer)labelID );
	return 1;
}

static int lua_LoadFont( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, pixelHeight, p, error );
	LUA_GET_STRING( ls, fileName, p, error );

	if( pixelHeight <= 0 ) {
		llog( LOG_WARN, "Pixel height negative, setting to 12." );
		pixelHeight = 12;
	}

	int font = txt_LoadFont( fileName, pixelHeight );

	lua_pushinteger( ls, font );
	return 1;
}

static int lua_GetSpriteSheetImage( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, imageIdx, p, error );
	LUA_GET_INT( ls, packageID, p, error );

	int* sbImgs = img_GetPackageImages( packageID );

	int img = -1;
	if( imageIdx > 0 && imageIdx < sb_Count( sbImgs ) ) {
		img = sbImgs[imageIdx];
	}

	lua_pushinteger( ls, img );
	return 1;
}

static int lua_BindKeyPress( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, funcName, p, error );
	LUA_GET_INT( ls, luaKeyCode, p, error );

	SDL_Keycode keyCode = (SDL_Keycode)luaKeyCode;

	input_BindScriptOnKeyPress( keyCode, funcName );

	return 0;
}

static int lua_BindKeyRelease( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, funcName, p, error );
	LUA_GET_INT( ls, luaKeyCode, p, error );

	SDL_Keycode keyCode = (SDL_Keycode)luaKeyCode;

	input_BindScriptOnKeyRelease( keyCode, funcName );

	return 0;
}

static int lua_BindMousePress( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, funcName, p, error );
	LUA_GET_INT( ls, luaButton, p, error );

	Uint8 button = (Uint8)luaButton;

	input_BindScriptOnMouseButtonPress( button, funcName );

	return 0;
}

static int lua_BindMouseRelease( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, funcName, p, error );
	LUA_GET_INT( ls, luaButton, p, error );

	Uint8 button = (Uint8)luaButton;

	input_BindScriptOnMouseButtonRelease( button, funcName );

	return 0;
}

static int lua_ClearKeyBinds( lua_State* ls )
{
	input_ClearAllKeyBinds( );
	return 0;
}

static int lua_ClearMouseButtonBinds( lua_State* ls )
{
	input_ClearAllMouseButtonBinds( );
	return 0;
}

static int lua_GetMousePosition( lua_State* ls )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );

	lua_pushnumber( ls, mousePos.x );
	lua_pushnumber( ls, mousePos.y );

	return 2;
}

static int lua_AddGroupID( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, groupID, p, error );
	LUA_GET_INT( ls, entityID, p, error );

	gp_AddGroupIDToEntityAndChildren( &defaultECPS, (EntityID)entityID, (uint32_t)groupID );

	return 0;
}

static int lua_DestroyEntitiesByGroupID( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, groupID, p, error );

	gp_DeleteAllOfGroup( &defaultECPS, groupID );

	return 0;
}

static int lua_LoadAnimation( lua_State* ls )
{
	// file name
	int p = -1;
	int error;

	LUA_GET_STRING( ls, fileName, p, error );

	int anim = sprAnim_LoadAsResource( fileName );

	lua_pushinteger( ls, anim );
	return 1;
}

static int lua_DestroyEntity( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, id, p, error );

	ecps_DestroyEntityByID( &defaultECPS, id );

	return 0;
}

static int lua_GetEntityPosition( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, id, p, error );

	GCTransformData* tf;
	if( ecps_GetComponentFromEntityByID( &defaultECPS, id, gcTransformCompID, &tf ) ) {
		Vector2 pos = gc_GetLocalPos( tf );
		lua_pushboolean( ls, 1 );
		lua_pushnumber( ls, pos.x );
		lua_pushnumber( ls, pos.y );
		return 3;
	}

	lua_pushboolean( ls, 0 );
	return 1;
}

static int lua_GetImageByID( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, id, p, error );
	int imageID = img_GetExistingByID( id );

	lua_pushinteger( ls, imageID );
	return 1;
}

static int lua_RenderImagePos( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_INT( ls, depth, p, error );
	LUA_GET_INT( ls, camFlags, p, error );
	LUA_GET_FLOAT( ls, y, p, error );
	LUA_GET_FLOAT( ls, x, p, error );
	LUA_GET_INT( ls, imgID, p, error );

	if( depth < INT8_MIN ) {
		llog( LOG_WARN, "Depth too low, clamping to minimum value." );
		depth = INT8_MIN;
	}

	if( depth > INT8_MAX ) {
		llog( LOG_WARN, "Depth to high, clamping to maximum value." );
		depth = INT8_MAX;
	}

	Vector2 pos = vec2( x, y );
	img_Render_Pos( imgID, camFlags, (int8_t)depth, &pos );

	return 0;
}
//************************************************************************************

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
// vc++ is reporting that the failure branch is never reached
#pragma warning( push )
#pragma warning( disable : 4702 )
bool xLua_GetNextInt( lua_State* ls, int* p, int* out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( !lua_isinteger( ls, *p ) ) {
		// error
		( *out ) = 0;
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = (int)lua_tointeger( ls, *p );
		--( *p );
		return true;
	}
}
#pragma warning( pop )

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
// vc++ is reporting that the failure branch is never reached
#pragma warning( push )
#pragma warning( disable : 4702 )
bool xLua_GetNextString( lua_State* ls, int* p, const char** out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( lua_isstring( ls, *p ) ) {
		( *out ) = lua_tostring( ls, *p );
		--( *p );
		return true;
	} else if( lua_isnil( ls, *p ) ) {
		( *out ) = NULL;
		--( *p );
		return true;
	} else {
		// error
		( *out ) = NULL;
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	}
}
#pragma warning( pop )

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
// vc++ is reporting that the failure branch is never reached
#pragma warning( push )
#pragma warning( disable : 4702 )
bool xLua_GetNextFloat( lua_State* ls, int* p, float* out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );
	if( !lua_isnumber( ls, *p ) ) {
		// error
		( *out ) = 0.0f;
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = (float)lua_tonumber( ls, *p );
		--( *p );
		return true;
	}
}
#pragma warning( pop )

// returns if it was a success
//  if it was then out should now have a valid value, if not then errorOut should have a valid value
// vc++ is reporting that the failure branch is never reached
#pragma warning( push )
#pragma warning( disable : 4702 )
bool xLua_GetNextBool( lua_State* ls, int* p, bool* out, int* errorOut, const char* name )
{
	SDL_assert( ls != NULL );
	SDL_assert( p != NULL );
	SDL_assert( out != NULL );
	SDL_assert( errorOut != NULL );

	if( !lua_isboolean( ls, *p ) ) {
		// error
		( *out ) = false;
		( *errorOut ) = luaL_error( ls, "Invalid type for %s", name );
		return false;
	} else {
		( *out ) = (bool)lua_toboolean( ls, *p );
		--( *p );
		return true;
	}
}
#pragma warning( pop )

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

static int loadAndRunFile( lua_State* ls )
{
	int p = -1;
	int error;

	LUA_GET_STRING( ls, fileName, p, error );
	if( SDL_strlen( fileName ) == 0 ) {
		lua_pushnil( ls );
		return 1;
	}

	bool isValid = false;
#ifndef _DEBUG
	// if it's a debug executable we're assuming noone but developers have access to it
	isValid = true;
#else
	// make sure the file is valid, it should be local to the directory the executable is running in
	const char* dir = SDL_GetBasePath( );
#endif	


}

static int loadAndRunFileOnce( lua_State* ls )
{
	// look into ll_require( to see how lua does it, probably want to copy close to that with additional safety checks
	int p = -1;
	int error;

	LUA_GET_STRING( ls, fileName, p, error );
	if( SDL_strlen( fileName ) == 0 ) {
		lua_pushnil( ls );
		return 1;
	}

	// see if the file is already loaded, if it is then don't do it
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

	// make sure the file is local to the directory where the executable is being run from

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
	{ LUA_DBLIBNAME, luaopen_debug }, // should be safe as only the dev will have debug builds
#endif
	{ NULL, NULL }
};

static const char* disabledFunctions[] = {
	"require",
	"load",
	"loadfile",
	"loadstring",
	"gcinfo",
	"collectgarbage",
	"dofile",
	"module",
	"loadlib"
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

	// disable some individual things that would break the sandbox
	// using this as a reference, understanding we don't need to worry about the global state: https://lua-users.org/wiki/SandBoxes
	/*for( size_t i = 0; i < ARRAY_SIZE( disabledFunctions ); ++i ) {
		lua_pushnil( luaState );
		lua_setglobal( luaState, disabledFunctions[i] );
	}//*/

	// load the main lua file that has our base engine lua functions
	if( !xLua_LoadAndDoFile( "Scripts/main.lua " ) ) {
		llog( LOG_ERROR, "Unable to load Scripts/main.lua." );
		return false;
	}

	// register some functions that Lua scripts can use, probably want to pull these out into a separate file eventually
	xLua_RegisterCFunction( "setCameraFlags", lua_SetCamFlags );
	xLua_RegisterCFunction( "setClearColor", lua_SetClearColor );
	xLua_RegisterCFunction( "loadImage", lua_LoadImage );
	xLua_RegisterCFunction( "createSprite", lua_CreateSprite );
	xLua_RegisterCFunction( "loadStreamingSound", lua_LoadStreamingSound );
	xLua_RegisterCFunction( "playStreamingSound", lua_PlayStreamingSound );
	xLua_RegisterCFunction( "loadSound", lua_LoadSound );
	xLua_RegisterCFunction( "playSound", lua_PlaySound );
	xLua_RegisterCFunction( "loadSpriteSheet", lua_LoadSpriteSheet );
	xLua_RegisterCFunction( "createButton", lua_CreateButton );
	xLua_RegisterCFunction( "createLabel", lua_CreateLabel );
	xLua_RegisterCFunction( "loadFont", lua_LoadFont );
	xLua_RegisterCFunction( "getSpriteSheetImage", lua_GetSpriteSheetImage );
	xLua_RegisterCFunction( "bindKeyPress", lua_BindKeyPress );
	xLua_RegisterCFunction( "bindKeyRelease", lua_BindKeyRelease );
	xLua_RegisterCFunction( "bindMouseButtonPress", lua_BindMousePress );
	xLua_RegisterCFunction( "bindMouseButtonRelease", lua_BindMouseRelease );
	xLua_RegisterCFunction( "clearKeyBinds", lua_ClearKeyBinds );
	xLua_RegisterCFunction( "clearMouseButtonBinds", lua_ClearMouseButtonBinds );
	xLua_RegisterCFunction( "getMousePosition", lua_GetMousePosition );
	xLua_RegisterCFunction( "addGroupID", lua_AddGroupID );
	xLua_RegisterCFunction( "destroyEntitiesByGroupID", lua_DestroyEntitiesByGroupID );
	xLua_RegisterCFunction( "loadAnimation", lua_LoadAnimation );
	xLua_RegisterCFunction( "destroyEntity", lua_DestroyEntity );
	xLua_RegisterCFunction( "getEntityPosition", lua_GetEntityPosition );
	xLua_RegisterCFunction( "getImageByID", lua_GetImageByID );
	xLua_RegisterCFunction( "renderImageAtPos", lua_RenderImagePos );

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