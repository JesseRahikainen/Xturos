#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#include "jobRingQueue.h"

// Simple job queue system to handle multithreading
//  Primarily issue is how to handle data passing and allocation, will need to make memory manager thread safe
//  Easy way may to be do a memory pool per thread
//  Initial test will be with threaded loading of assets
// Stores the jobs in a priority queue
int jq_Initialize( uint8_t numThreads );
void jq_ShutDown( void );
bool jq_AddJob( JobProcessFunc proc, void* data );
bool jq_AddMainThreadJob( JobProcessFunc proc, void* data );

// gets the next job and runs it, used if you want the main thread running jobs as well
bool jq_ProcessNextJob( void );

// Returns if all the non-main thread jobs are done
bool jq_AllJobsDone( void );

// Goes through all the jobs added to the main thread and processes them
//  If there is no threading support then all other jobs are processed here as well
void jq_ProcessMainThreadJobs( void );

#endif /* inclusion guard */