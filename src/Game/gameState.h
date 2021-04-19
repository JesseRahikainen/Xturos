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

void gsm_EnterState( GameStateMachine* fsm, GameState* newState );
void gsm_ProcessEvents( GameStateMachine* fsm, SDL_Event* e );
void gsm_Process( GameStateMachine* fsm );
void gsm_Draw( GameStateMachine* fsm );
void gsm_PhysicsTick( GameStateMachine* fsm, float dt );

#endif
