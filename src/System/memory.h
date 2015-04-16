#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

int initMemory( size_t totalSize );
void cleanUpMemory( void );

void* mem_allocate( size_t size );
void* mem_resize( void* memory, size_t newSize );
void mem_release( void* memory );

#endif // inclusion guard