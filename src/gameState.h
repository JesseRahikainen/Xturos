#include <SDL_events.h>

#ifndef ENGINE_GAMESTATE_H
#define ENGINE_GAMESTATE_H

struct GameState {
	int (*enter)(void);
	int (*exit)(void);

	void (*processEvents)(SDL_Event*);
	void (*process)(void);
	void (*draw)(void);
	void (*physicsTick)(float);

	struct GameState* childState;
};

struct GameStateMachine {
	struct GameState* currentState;
};

struct GameStateMachine globalFSM;

void gsmEnterState( struct GameStateMachine* fsm, struct GameState* newState );
void gsmProcessEvents( struct GameStateMachine* fsm, SDL_Event* e );
void gsmProcess( struct GameStateMachine* fsm );
void gsmDraw( struct GameStateMachine* fsm );
void gsmPhysicsTick( struct GameStateMachine* fsm, float dt );

#endif
