#ifndef INITIAL_CHOICE_STATE_STATE_H
#define INITIAL_CHOICE_STATE_STATE_H

#include "gameState.h"

void initialChoice_RegisterState( const char* name, GameState* state, bool pushedState );

extern GameState initialChoiceState;

#endif // inclusion guard
