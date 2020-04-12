#include "gameState.h"

#include <assert.h>

// this is a simple state machine for right now, will most likely modify it to be heirarchical at some point
//  but it's not necessary now

GameStateMachine globalFSM = { NULL };

void gsm_EnterState( GameStateMachine* fsm, GameState* newState )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->exit != NULL ) ) {
		fsm->currentState->exit( );
	}

	if( ( newState != NULL ) && ( newState->enter != NULL ) ) {
		newState->enter( );
	}

	fsm->currentState = newState;
}

void gsm_ProcessEvents( GameStateMachine* fsm, SDL_Event* e )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->process != NULL ) ) {
		fsm->currentState->processEvents( e );
	}
}

void gsm_Process( GameStateMachine* fsm )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->process != NULL ) ) {
		fsm->currentState->process( );
	}
}

void gsm_Draw( GameStateMachine* fsm )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->draw != NULL ) ) {
		fsm->currentState->draw( );
	}
}

void gsm_PhysicsTick( GameStateMachine* fsm, float dt )
{
	if( ( fsm->currentState != NULL ) && ( fsm->currentState->physicsTick != NULL ) ) {
		fsm->currentState->physicsTick( dt );
	}
}