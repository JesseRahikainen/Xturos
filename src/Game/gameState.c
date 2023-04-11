#include "gameState.h"

#include <assert.h>

#include "Utils/stretchyBuffer.h"

// this is a simple state machine for right now, will most likely modify it to be heirarchical at some point
//  but it's not necessary now

GameStateMachine globalFSM = { NULL };

void gsm_EnterState( GameStateMachine* fsm, GameState* newState )
{
	assert( fsm );

	gsm_PopState( fsm );
	gsm_PushState( fsm, newState );
}

void gsm_PushState( GameStateMachine* fsm, GameState* newState )
{
	assert( fsm );
	
	if( ( newState != NULL ) && ( newState->enter != NULL ) ) {
		newState->enter( );
	}
	sb_Push( fsm->sbStateStack, newState );
}

void gsm_PopState( GameStateMachine* fsm )
{
	assert( fsm );

	if( sb_Count( fsm->sbStateStack ) > 0 ) {
		GameState* state = sb_Pop( fsm->sbStateStack );
		if( ( state != NULL ) && ( state->exit != NULL ) ) {
			state->exit( );
		}
	}
}

void gsm_ProcessEvents( GameStateMachine* fsm, SDL_Event* e )
{
	assert( fsm );

	GameState* state = sb_Last( fsm->sbStateStack );
	if( ( state != NULL ) && ( state->processEvents != NULL ) ) {
		state->processEvents( e );
	}
}

void gsm_Process( GameStateMachine* fsm )
{
	assert( fsm );

	GameState* state = sb_Last( fsm->sbStateStack );
	if( ( state != NULL ) && ( state->process != NULL ) ) {
		state->process( );
	}
}

void gsm_Draw( GameStateMachine* fsm )
{
	assert( fsm );

	// draw in reverse order
	for( size_t i = 0; i < sb_Count( fsm->sbStateStack ); ++i ) {
		GameState* state = fsm->sbStateStack[i];
		if( ( state != NULL ) && ( state->draw != NULL ) ) {
			state->draw( );
		}
	}
}

void gsm_PhysicsTick( GameStateMachine* fsm, float dt )
{
	assert( fsm );

	GameState* state = sb_Last( fsm->sbStateStack );
	if( ( state != NULL ) && ( state->physicsTick != NULL ) ) {
		state->physicsTick( dt );
	}
}