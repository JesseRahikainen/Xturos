#include "jobQueue.h"

#include <SDL3/SDL_assert.h>
#include <string.h>

#include "memory.h"

int jrq_Init( JobRingQueue* queue, size_t size )
{
	SDL_assert( queue != NULL );
	SDL_assert( size > 0 );

	queue->size = size;
	queue->ringBuffer = mem_Allocate( sizeof( queue->ringBuffer[0] ) * size );
	if( queue->ringBuffer == NULL ) {
		return -1;
	}
	memset( queue->ringBuffer, 0, size * sizeof( queue->ringBuffer[0] ) );
	SDL_SetAtomicInt( &( queue->head ), 0 );
	SDL_SetAtomicInt( &( queue->tail ), 0 );
	SDL_SetAtomicInt( &( queue->busy ), 0 );

	return 0;
}

void jrq_CleanUp( JobRingQueue* queue )
{
	SDL_assert( queue != NULL );

	mem_Release( queue->ringBuffer );
}

void jrq_Write( JobRingQueue* queue, Job* jobby )
{
	// TODO: Test to see if we're writing over the tail, and if we are then fail
	bool writeSuccess = false;
	while( !writeSuccess ) {
		int idx = queue->head.value;
		if( SDL_CompareAndSwapAtomicInt( &( queue->head ), idx, ( idx + 1 ) % queue->size ) ) {
			queue->ringBuffer[idx] = (*jobby);
			writeSuccess = true;
		}
	}
}

// do the next job available in the ring buffer, returns if anything was actually done
bool jrq_ProcessNext( JobRingQueue* queue )
{
	int idx = queue->tail.value;
	if( idx != queue->head.value ) {
		if( SDL_CompareAndSwapAtomicInt( &( queue->tail ), idx, ( idx + 1 ) % queue->size ) ) {
			SDL_AddAtomicInt( &( queue->busy ), 1 );

			if( queue->ringBuffer[idx].process != NULL ) queue->ringBuffer[idx].process( queue->ringBuffer[idx].data );
			queue->ringBuffer[idx].process = NULL; // invalidate the job

			SDL_AddAtomicInt( &( queue->busy ), -1 );

			return true;
		}
	}
	return false;
}

bool jrq_IsEmpty( JobRingQueue* queue )
{
	return ( queue->head.value == queue->tail.value );
}

bool jrq_IsBusy( JobRingQueue* queue )
{
	return ( queue->busy.value > 0 );
}