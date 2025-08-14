#include "initialChoiceState.h"

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "Utils/stretchyBuffer.h"
#include "IMGUI/nuklearWrapper.h"

typedef struct {
	const char* name;
	GameState* state;
} RegisteredState;
static RegisteredState* states = NULL;

void initialChoice_RegisterState( const char* name, GameState* state )
{
	RegisteredState newState = { name, state };
	sb_Push( states, newState );
}

static void initialChoiceState_Enter( void )
{
}

static void initialChoiceState_Exit( void )
{
}

static void initialChoiceState_ProcessEvents( SDL_Event* e )
{
}

static void initialChoiceState_Process( void )
{
	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect windowBounds = { 0.0f, 0.0f, (float)renderWidth, (float)renderHeight };
	const int NUM_COLS = 6;
	if( nk_begin( ctx, "Initial Choice", windowBounds, 0 ) ) {
		for( size_t row = 0; row < ( ( sb_Count( states ) / NUM_COLS ) + 1 ); ++row ) {
			nk_layout_row_begin( ctx, NK_STATIC, 30, NUM_COLS ); {
				for( size_t col = 0; col < NUM_COLS; ++col ) {
					size_t idx = ( row * NUM_COLS ) + col;
					if( idx < sb_Count( states ) ) {
						nk_layout_row_push( ctx, 100.0f );
						if( nk_button_label( ctx, states[idx].name ) ) {
							gsm_EnterState( &globalFSM, states[idx].state );
						}
					}
				}
			} nk_layout_row_end( ctx );
		}
	} nk_end( ctx );
}

static void initialChoiceState_Draw( void )
{

}

static void initialChoiceState_PhysicsTick( float dt )
{
}

GameState initialChoiceState = { initialChoiceState_Enter, initialChoiceState_Exit, initialChoiceState_ProcessEvents,
	initialChoiceState_Process, initialChoiceState_Draw, initialChoiceState_PhysicsTick };
