#include <SDL_events.h>

#ifndef ENGINE_GAMESTATE_H
#define ENGINE_GAMESTATE_H

typedef struct _GameState {
	void (*enter)(void);
	void (*exit)(void);

	void (*processEvents)(SDL_Event*);	// happens whenever an event is recieved, passes in the event
	void (*process)(void);				// happens once per frame
	void (*draw)(void);					// happens after one or more physics ticks happen
	void (*physicsTick)(float);			// happens whenever a physics ticks happens, passes in physics time delta
	void (*render)(float);				// happens once a frame, passes in the normalized time since the last physics tick
} GameState;

typedef struct {
	GameState** sbStateStack;
} GameStateMachine;

extern GameStateMachine globalFSM;

void gsm_EnterState( GameStateMachine* fsm, GameState* newState );
void gsm_PushState( GameStateMachine* fsm, GameState* newState );
void gsm_PopState( GameStateMachine* fsm );
void gsm_ProcessEvents( GameStateMachine* fsm, SDL_Event* e );
void gsm_Process( GameStateMachine* fsm );
void gsm_Draw( GameStateMachine* fsm );
void gsm_PhysicsTick( GameStateMachine* fsm, float dt );
void gsm_Render( GameStateMachine* fsm, float normRenderTime );

#endif
