#include <SDL_events.h>

#ifndef ENGINE_GAMESTATE_H
#define ENGINE_GAMESTATE_H

typedef struct _GameState {
	int (*enter)(void);
	int (*exit)(void);

	void (*processEvents)(SDL_Event*);
	void (*process)(void);
	void (*draw)(void);
	void (*physicsTick)(float);

	struct _GameState* childState;
} GameState;

typedef struct {
	GameState* currentState;
} GameStateMachine;

GameStateMachine globalFSM;

void gsmEnterState( GameStateMachine* fsm, GameState* newState );
void gsmProcessEvents( GameStateMachine* fsm, SDL_Event* e );
void gsmProcess( GameStateMachine* fsm );
void gsmDraw( GameStateMachine* fsm );
void gsmPhysicsTick( GameStateMachine* fsm, float dt );

#endif
