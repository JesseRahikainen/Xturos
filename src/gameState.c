#include "gameState.h"

#include <assert.h>

// this is a simple state machine for right now, will most likely modify it to be heirarchical at some point
//  but it's not necessary now

struct GameStateMachine globalGSM = { NULL };

void gsmEnterState( struct GameStateMachine* fsm, struct GameState* newState )
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

void gsmProcessEvents( struct GameStateMachine* fsm, SDL_Event* e )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->process != NULL ) ) {
		fsm->currentState->processEvents( e );
	}
}

void gsmProcess( struct GameStateMachine* fsm )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->process != NULL ) ) {
		fsm->currentState->process( );
	}
}

void gsmDraw( struct GameStateMachine* fsm )
{
	assert( fsm );

	if( ( fsm->currentState != NULL ) && ( fsm->currentState->draw != NULL ) ) {
		fsm->currentState->draw( );
	}
}

void gsmPhysicsTick( struct GameStateMachine* fsm, float dt )
{
	if( ( fsm->currentState != NULL ) && ( fsm->currentState->physicsTick != NULL ) ) {
		fsm->currentState->physicsTick( dt );
	}
}