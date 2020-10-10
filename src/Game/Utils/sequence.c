#include "sequence.h"

#include <stdarg.h>

#include "stretchyBuffer.h"

void sequence_Init( Sequence* seq, void* data, size_t numSteps, ... )
{
	seq->timeLeft = -1.0f;
	seq->data = data;
	seq->currStep = 0;

	seq->sbSteps = NULL;
	va_list list;
	va_start( list, numSteps ); {
		for( size_t i = 0; i < numSteps; ++i ) {
			sb_Push( seq->sbSteps, va_arg( list, SequenceStep ) );
		}
	} va_end( list );
}

void sequence_Reset( Sequence* seq )
{
	seq->timeLeft = -1.0f;
	seq->currStep = 0;
}

void sequence_CleanUp( Sequence* seq )
{
	sb_Release( seq->sbSteps );
}

// returns if the sequence has run it's course
bool sequence_Run( Sequence* sequence, float dt )
{
	assert( sequence != NULL );

	sequence->timeLeft -= dt;
	while( ( sequence->timeLeft <= 0.0f ) && ( sequence->currStep < sb_Count( sequence->sbSteps ) ) ) {
		bool done = false;
		sequence->timeLeft = sequence->sbSteps[sequence->currStep]( sequence->data, &done );
		if( done ) {
			++sequence->currStep;
		}
	}

	return ( sequence->currStep >= sb_Count( sequence->sbSteps ) );
}