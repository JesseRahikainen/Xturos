#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

int mem_Init( size_t totalSize );
void mem_CleanUp( void );
void mem_Log( void );
void mem_Verify( void );
int mem_GetVerify( void );
void mem_VerifyPointer( void* p );
void mem_Report( void );
void mem_GetReportValues( size_t* totalOut, size_t* inUseOut, size_t* overheadOut, uint32_t* fragmentsOut );

#define mem_Allocate( s ) mem_Allocate_Data( (s), __FILE__, __LINE__ )
#define mem_Resize( p, s ) mem_Resize_Data( (p), (s), __FILE__, __LINE__ )
#define mem_Release( p ) mem_Release_Data( (p), __FILE__, __LINE__ )

void* mem_Allocate_Data( size_t size, const char* fileName, const int line );
void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line );
void mem_Release_Data( void* memory, const char* fileName, const int line );

void mem_RunTests( void );

#define MEM_VERIFY_BLOCK( f ) { mem_Verify( ); f; mem_Verify( ); }

#endif // inclusion guard