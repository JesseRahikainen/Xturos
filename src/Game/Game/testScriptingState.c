#include "testScriptingState.h"

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "IMGUI/nuklearWrapper.h"
#include "System/luaInterface.h"

#include "System/serializer.h"

typedef struct {
	int32_t testInteger;
	Vector2 vec;
	char* string;
} TestSerializationStructure;

static void serializeVec2( Serializer* s, const char* name, Vector2* vec )
{
	s->startStructure( s, name ); {
		s->flt( s, "x", &(vec->x) );
		s->flt( s, "y", &(vec->y) );
	} s->endStructure( s, name );
}

static void serializeTestStructure( Serializer* s, const char* name, TestSerializationStructure* st )
{
	s->startStructure( s, name ); {
		s->s32( s, "testInteger", &(st->testInteger) );
		s->cString( s, "string", &(st->string) );
		vec2_Serialize( s, "vec", &( st->vec ) );
	} s->endStructure( s, name );
}

static int runSerializeTest( lua_State* ls )
{
	Vector2 testVector = { .x = 1.0f, .y = 2.0f };
	TestSerializationStructure test = { .testInteger = 42, .vec = testVector };
	test.string = createStringCopy( "Testing" );

	Serializer luaSerializer;
	serializer_CreateWriteLua( ls, &luaSerializer );
	serializeTestStructure( &luaSerializer, "", &test );

	// returning just the structure
	return 1;
}

static int runDeserializeTest( lua_State* ls )
{
	TestSerializationStructure test;
	test.string = NULL;

	Serializer luaDeserializer;
	serializer_CreateReadLua( ls, &luaDeserializer );
	serializeTestStructure( &luaDeserializer, "", &test );

	llog( LOG_DEBUG, "Result - string: %s, testInteger: %i, x: %f, y: %f", test.string, test.testInteger, test.vec.x, test.vec.y );
	mem_Release( test.string );

	return 0;
}

static void testScriptingState_Enter( void )
{
	xLua_LoadAndDoFile( "Scripts/testScripting.lua" );

	xLua_RegisterCFunction( "testGetStructure", runSerializeTest );
	xLua_RegisterCFunction( "sendTestStructure", runDeserializeTest );
}

static void testScriptingState_Exit( void )
{
}

static void testScriptingState_ProcessEvents( SDL_Event* e )
{
}

static void testScriptingState_Process( void )
{
	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect bounds = nk_rect( 0.0f, 0.0f, 200.0f, 200.0f );
	nk_begin( ctx, "Commands", bounds, NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR ); {
		nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 );
		nk_layout_row_push( ctx, 1.0f );
		if( nk_button_label( ctx, "Create Test Entity" ) ) {
			xLua_CallLuaFunction( "createTestEntity", "" );
		}

		nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 );
		nk_layout_row_push( ctx, 1.0f );
		if( nk_button_label( ctx, "Destroy Test Entities" ) ) {
			xLua_CallLuaFunction( "destroyAllTestEntities", "" );
		}

		nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 );
		nk_layout_row_push( ctx, 1.0f );
		if( nk_button_label( ctx, "Test Send Structure To Lua" ) ) {
			xLua_CallLuaFunction( "testStructSerialize", "" );
		}

		nk_layout_row_begin( ctx, NK_DYNAMIC, 20, 1 );
		nk_layout_row_push( ctx, 1.0f );
		if( nk_button_label( ctx, "Test Receive Structure From Lua" ) ) {
			xLua_CallLuaFunction( "testStructureDeserialize", "" );
		}
	} nk_end( ctx );
}

static void testScriptingState_Draw( void )
{
}

static void testScriptingState_PhysicsTick( float dt )
{
}

static void testScriptingState_Render( float t )
{
}

GameState testScriptingState = { testScriptingState_Enter, testScriptingState_Exit, testScriptingState_ProcessEvents,
	testScriptingState_Process, testScriptingState_Draw, testScriptingState_PhysicsTick, testScriptingState_Render };
