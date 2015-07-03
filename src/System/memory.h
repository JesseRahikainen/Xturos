#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

int mem_Init( size_t totalSize );
void mem_CleanUp( void );
void mem_Log( void );

#define mem_Allocate( s ) mem_Allocate_Data( (s), __FILE__, __LINE__ )
#define mem_Resize( p, s ) mem_Resize_Data( (p), (s), __FILE__, __LINE__ )
#define mem_Release( p ) mem_Release_Data( (p), __FILE__, __LINE__ )

void* mem_Allocate_Data( size_t size, const char* fileName, const int line );
void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line );
void mem_Release_Data( void* memory, const char* fileName, const int line );

#endif // inclusion guard