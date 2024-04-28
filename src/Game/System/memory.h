#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// we'll make this the main memory thing, then we'll have memArena_* functions that that work with a MemoryArena struct.
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

bool mem_IsAllocatedMemory( void* ptr );

#define mem_Allocate( s ) mem_Allocate_Data( (s), __FILE__, __LINE__ )
#define mem_Resize( p, s ) mem_Resize_Data( (p), (s), __FILE__, __LINE__ )
#define mem_Release( p ) mem_Release_Data( (p), __FILE__, __LINE__ )

void* mem_Allocate_Data( size_t size, const char* fileName, const int line );
void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line );
void mem_Release_Data( void* memory, const char* fileName, const int line );

// attach the memory pointed to by child to parent, so that if parent is released so is the child memory
//  will overwrite the current parent of child if one exists, returns if the attach was successful
bool mem_Attach( void* parent, void* child );
void mem_DetachFromParent( void* child );
void mem_DetachAllChildren( void* parent );

void mem_RunTests( void );

#define MEM_VERIFY_BLOCK( f ) { mem_Verify( ); f; mem_Verify( ); }

#endif // inclusion guard