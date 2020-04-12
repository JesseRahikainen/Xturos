#include "jobQueue.h"

#include <stddef.h>
#include <SDL.h>
#include <assert.h>

#include "../System/platformLog.h"
#include "../Utils/stretchyBuffer.h"

// TODO?: Give the option to create multiple job queues

static JobRingQueue jobQueue;

static JobRingQueue mainThreadQueue; // used for things that need to be done on the main thread

// returns if all the jobs are done or not
bool jq_AllJobsDone( void )
{
	return ( jrq_IsEmpty( &jobQueue ) && !jrq_IsBusy( &jobQueue ) );
}

// non-static version for if we want the main thread to process jobs as well
bool jq_ProcessNextJob( void )
{
	return jrq_ProcessNext( &jobQueue );
}

static SDL_sem* jobQueueSemaphore = NULL;
static SDL_atomic_t quitFlag;
static SDL_Thread** sbThreadPool = NULL;

#include <stdio.h>
static int jobThread( void* data )
{
	// check for new job
	while( quitFlag.value == 0 ) {
		if( !jrq_ProcessNext( &jobQueue ) ) {
			// no job to process, wait until more jobs are added
			SDL_SemWait( jobQueueSemaphore );
		}
	}

	return 0;
}

int jq_Initialize( uint8_t numThreads )
{
	assert( numThreads > 0 );

	sbThreadPool = NULL;
	jobQueue.ringBuffer = NULL;
	mainThreadQueue.ringBuffer = NULL;

	if( jrq_Init( &jobQueue, 256 ) < 0 ) {
		llog( LOG_ERROR, "Unable to create job ring queue." );
		jq_ShutDown( );
		return -1;
	}

	if( jrq_Init( &mainThreadQueue, 256 ) < 0 ) {
		llog( LOG_ERROR, "Unable to create main thread job queue." );
		jq_ShutDown( );
		return -1;
	}

#ifdef THREAD_SUPPORT
	SDL_AtomicSet( &quitFlag, 0 );

	jobQueueSemaphore = SDL_CreateSemaphore( 0 );
	if( jobQueueSemaphore == NULL ) {
		llog( LOG_ERROR, "Unable to create job queue semaphore: %s", SDL_GetError( ) );
		jq_ShutDown( );
		return -1;
	}

	sb_Add( sbThreadPool, numThreads );
	if( sbThreadPool == NULL ) {
		llog( LOG_ERROR, "Unable to create thread pool!" );
		jq_ShutDown( );
		return -1;
	}

	size_t numThreadsCreated = 0;
	for( size_t i = 0; i < sb_Count( sbThreadPool ); ++i ) {
		char name[16];
		SDL_snprintf( name, SDL_arraysize( name ), "Wrkr_%i", i );
		sbThreadPool[i] = SDL_CreateThread( jobThread, name, NULL );
		if( sbThreadPool[i] == NULL ) {
			llog( LOG_WARN, "Unable to create thread %i! Will continue with fewer threads. Reason: %s", i, SDL_GetError( ) );
		} else {
			++numThreadsCreated;
		}
	}

	if( numThreadsCreated == 0 ) {
		llog( LOG_ERROR, "Unable to create any threads for pool!" );
		jq_ShutDown( );
		return -1;
	}
#else
	llog( LOG_INFO, "Compiled without support for threads, all jobs will be run on main thread." );
#endif

	return 0;
}

void jq_ShutDown( void )
{
#ifdef THREAD_SUPPORT
	// signal to the threads that they need to shut down
	SDL_AtomicSet( &quitFlag, 1 );

	// wait for all the threads to shut down
	for( size_t i = 0; i < sb_Count( sbThreadPool ); ++i ) {
		SDL_SemPost( jobQueueSemaphore ); // get the threads to wake up

		// don't need the return value, so don't bother waiting
		SDL_DetachThread( sbThreadPool[i] );
	}

	// destroy the thread pool
	sb_Release( sbThreadPool );

	SDL_DestroySemaphore( jobQueueSemaphore );
	jobQueueSemaphore = NULL;
#endif

	jrq_CleanUp( &mainThreadQueue );
	jrq_CleanUp( &jobQueue );
}

static void addJob( JobProcessFunc proc, void* data, JobRingQueue* queue )
{
	Job newJob;
	newJob.process = proc;
	newJob.data = data;

	jrq_Write( queue, &newJob );
}

// TODO: Create a copy of the data so we don't have to worry about it disappearing while
//  it's in use.
bool jq_AddJob( JobProcessFunc proc, void* data )
{
	// trying to use these generates fatal error C1001, so fucking MSVC won't let us do any error checking...
	//if( proc == NULL ) return false;
	/*if( sbJobQueue == NULL ) {
		llog( LOG_WARN, "Attempting to add job before job queue created." );
		return false;
	}//*/
	addJob( proc, data, &jobQueue );
	SDL_SemPost( jobQueueSemaphore );

	return true;
}

bool jq_AddMainThreadJob( JobProcessFunc proc, void* data )
{
	addJob( proc, data, &mainThreadQueue );
	return true;
}

// Goes through all the jobs added to the main thread and processes them
//  If there is no threading support then all other jobs are processed here as well
void jq_ProcessMainThreadJobs( void )
{
#ifndef THREAD_SUPPORT
	while( jrq_ProcessNext( &jobQueue ) )
		;
#endif

	while( jrq_ProcessNext( &mainThreadQueue ) )
		;
}