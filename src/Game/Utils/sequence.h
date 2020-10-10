#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <stdbool.h>
#include <stdlib.h>

typedef float ( *SequenceStep )( void* data, bool* outDone );

typedef struct {
	SequenceStep* sbSteps;
	size_t currStep;
	void* data;
	float timeLeft;
} Sequence;

void sequence_Init( Sequence* seq, void* data, size_t numSteps, ... );

void sequence_Reset( Sequence* seq );

void sequence_CleanUp( Sequence* seq );

// returns if the sequence has run it's course
bool sequence_Run( Sequence* sequence, float dt );

#endif // SEQUENCE_H