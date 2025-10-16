#include "initialChoiceState.h"

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "Graphics/camera.h"
#include "Graphics/images.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "Utils/stretchyBuffer.h"
#include "IMGUI/nuklearWrapper.h"
#include "Input/input.h"

typedef struct {
	const char* name;
	GameState* state;
	bool pushedState;
} RegisteredState;
static RegisteredState* states = NULL;

static int whiteImg = INVALID_IMAGE_ID;

void initialChoice_RegisterState( const char* name, GameState* state, bool pushedState )
{
	RegisteredState newState = { name, state, pushedState };
	sb_Push( states, newState );
}

static void initialChoiceState_Enter( void )
{
	whiteImg = img_GetExistingByID( "default_white_square" );

	gfx_SetClearColor( CLR_BLUE );
	cam_SetFlags( 0, 1 );
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
							if( states[idx].pushedState ) {
								gsm_PushState( &globalFSM, states[idx].state );
							} else {
								gsm_EnterState( &globalFSM, states[idx].state );
							}
						}
					}
				}
			} nk_layout_row_end( ctx );
		}
	} nk_end( ctx );
}

static void initialChoiceState_Draw( void )
{
	//Vector2 mousePos;
	//input_GetMousePosition( &mousePos );
	//debugRenderer_Circle( 1, mousePos, 10.0f, CLR_BLUE );
}

static void initialChoiceState_PhysicsTick( float dt )
{
}

static void initialChoiceState_Render( float t )
{
	int width, height;
	gfx_GetRenderSize( &width, &height );

	Vector2 size = vec2( 32.0f, 32.0f );

	Vector2 pos[4];
	pos[0] = VEC2_ZERO;
	pos[1] = vec2( (float)width, 0.0f );
	pos[2] = vec2( 0.0f, (float)height );
	pos[3] = vec2( (float)width, (float)height );

	for( size_t i = 0; i < ARRAY_SIZE( pos ); ++i ) {
		img_Render_PosSizeVClr( whiteImg, 1, 0, &pos[i], &size, &CLR_WHITE );
	}
}

GameState initialChoiceState = { initialChoiceState_Enter, initialChoiceState_Exit, initialChoiceState_ProcessEvents,
	initialChoiceState_Process, initialChoiceState_Draw, initialChoiceState_PhysicsTick, initialChoiceState_Render };
