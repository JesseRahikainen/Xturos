#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: Make thread safe! Lets start out with the easiest way and just have a mutex we use around the various
//  memory functions.
/*
Need to make this thread safe! But how? With all the functionality I want for it, especially for debugging
 we'll need to use a mutex for each pool. Will also need internal versions of the logging functions that don't
 lock the mutex (assume it has already been locked at the top).
 We'll just wrap each function in mutex lock/unlock. Will be the quickest way to get this working.
 Will also need internal versions of the logging and debugging functions that don't lock the mutex.
*/

int mem_Init( size_t totalSize );
void mem_CleanUp( void );

void mem_Log( void );
void mem_LogAddressBlockData( void* ptr, const char* extra );
void mem_Verify( void );
bool mem_GetVerify( void );
void mem_VerifyPointer( void* p );
void mem_Report( void );
void mem_GetReportValues( size_t* totalOut, size_t* inUseOut, size_t* overheadOut, uint32_t* fragmentsOut );

void mem_WatchAddress( void* ptr );
void mem_UnWatchAddress( void* ptr );

#define mem_Allocate( s ) mem_Allocate_Data( (s), __FILE__, __LINE__ )
#define mem_Resize( p, s ) mem_Resize_Data( (p), (s), __FILE__, __LINE__ )
#define mem_Release( p ) mem_Release_Data( (p), __FILE__, __LINE__ )

void* mem_Allocate_Data( size_t size, const char* fileName, const int line );
void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line );
void mem_Release_Data( void* memory, const char* fileName, const int line );

void mem_RunTests( void );

#define MEM_VERIFY_BLOCK( f ) { mem_Verify( ); f; mem_Verify( ); }

#endif // inclusion guard