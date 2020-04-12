#ifndef JOB_RING_QUEUE_H
#define JOB_RING_QUEUE_H

#include <SDL_atomic.h>

typedef void (*JobProcessFunc)( void* );

typedef struct {
	JobProcessFunc process;
	void* data; // should we make a copy of the data to put in here?
} Job;

// fixed size, thread safe ring buffer based queue
typedef struct {
	size_t size;
	Job* ringBuffer;
	SDL_atomic_t head;
	SDL_atomic_t tail;
	SDL_atomic_t busy; // a count of how many jobs are currently being processed
} JobRingQueue;

int jrq_Init( JobRingQueue* queue, size_t size );
void jrq_CleanUp( JobRingQueue* queue );
void jrq_Write( JobRingQueue* queue, Job* jobby );
// do the next job available in the ring buffer, returns if anything was actually done
bool jrq_ProcessNext( JobRingQueue* queue );
bool jrq_IsEmpty( JobRingQueue* queue );
bool jrq_IsBusy( JobRingQueue* queue );

#endif /* inclusion guard */