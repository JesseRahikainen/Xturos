#include "memory.h"

#include <SDL_log.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define GUARD_VALUE 0xDEADBEEF
#define MIN_ALLOC_SIZE 16

#define IN_USE_FLAG ( 1 << 31 )

//#define TEST_CLEAR_VALUES

//#define LOG_MEMORY_ALLOCATIONS

// TODO: Get aligned memory allocation working.

// TODO: we can probably assume that there will never be a two unused blocks next to each other, unless we want some way to
//   reserve memory in the future, in which case we don't want to make that assumption

typedef struct MemoryBlockHeader {
	uint32_t guardValue;

	struct MemoryBlockHeader* next;
	struct MemoryBlockHeader* prev;

	uint32_t flags;
	size_t size;

	// last file and line in that file that modified this block
#ifdef LOG_MEMORY_ALLOCATIONS
	char file[256];
	int line;
#endif
} MemoryBlockHeader;

// TODO: have all the functions take in a Memory structure so we can do something like memory pools. But how to do that without
//   breaking having signatures similar to the standard c library memory allocation functions?
typedef struct {
	void* memory;
} Memory;

static Memory memoryBlock;

#ifdef TEST_CLEAR_VALUES
static void testingSetMemory( void* start, size_t size, uint8_t val )
{
	uint8_t* data = (uint8_t*)start;
	for( size_t i = 0; i < size; ++i ) {
		data[i] = val;
	}
}
#else
static void testingSetMemory( void* start, size_t size, uint8_t val ) { }
#endif

// assume header is never NULL
#ifdef LOG_MEMORY_ALLOCATIONS
static void SetMemoryBlockInfo( MemoryBlockHeader* header, const char* fileName, int line )
{
	int lastFileIdx = ( sizeof( header->file ) / sizeof( header->file[0] ) ) - 1;
	strncpy( header->file, fileName, lastFileIdx );
	header->file[lastFileIdx] = 0;
	header->line = line;
}
#else
static void SetMemoryBlockInfo( MemoryBlockHeader* header, const char* fileName, int line ) { }
#endif

// returns the remaining block after the condensation
static MemoryBlockHeader* condenseMemoryBlocks( MemoryBlockHeader* start, const char* fileName, int line )
{
	assert( start != NULL );
	assert( !( start->flags & IN_USE_FLAG ) );

	// find the earliest block that's not in use
	while( ( start->prev != NULL ) && !( start->prev->flags & IN_USE_FLAG ) ) {
		start = start->prev;
	}

	// going to the right, merge unused blocks
	while( ( start->next != NULL ) && !( start->next->flags & IN_USE_FLAG ) ) {
		SetMemoryBlockInfo( start, fileName, line );
		MemoryBlockHeader* nextHeader = start->next;
		start->size += nextHeader->size + sizeof( MemoryBlockHeader );
		start->next = nextHeader->next;
		if( start->next != NULL ) {
			start->next->prev = start;
		}
	}

	return start;
}

static MemoryBlockHeader* createNewBlock( void* start, MemoryBlockHeader* prev, MemoryBlockHeader* next, size_t size, const char* fileName, int line )
{
	MemoryBlockHeader* header = (MemoryBlockHeader*)start;

	header->guardValue = GUARD_VALUE;
	header->flags = 0;
	header->size = size;

	if( prev != NULL ) {
		prev->next = header;
	}
	header->prev = prev;

	if( next != NULL ) {
		next->prev = header;
	}
	header->next = next;

	SetMemoryBlockInfo( header, fileName, line );

	return header;
}

static void* growBlock( MemoryBlockHeader* header, size_t newSize, const char* fileName, int line )
{
	assert( header != NULL );
	assert( header->size < newSize );

	void* result = NULL;

	// two cases, one where there's enough room to just expand it, the other
	//  where we'll have to release the current and allocate a new position
	//  for it

	// scan the blocks after this block and see if all the free blocks after
	//  it allow enough space to expand
	size_t sizeAllowed = header->size;
	MemoryBlockHeader* scan = header->next;
	while( ( scan != NULL ) && !( scan->flags & IN_USE_FLAG ) ) {
		sizeAllowed += scan->size + sizeof( MemoryBlockHeader );
		scan = scan->next;
	}

	if( newSize < sizeAllowed ) {
		// claim new headers until we've filled our quota
		result = (void*)( header + 1 );
		scan = header->next;
		while( ( scan != NULL ) && !( scan->flags & IN_USE_FLAG ) ) {
			header->size += scan->size + sizeof( MemoryBlockHeader );
			scan = scan->next;
		}

		// see if there's enough left over to create a new block
		if( header->size >= ( newSize + sizeof( MemoryBlockHeader ) + MIN_ALLOC_SIZE ) ) {
			MemoryBlockHeader* nextHeader = createNewBlock( (void*)( (uint8_t*)result + newSize ),
				header, scan, header->size - newSize - sizeof( MemoryBlockHeader ),
				fileName, line );
			header->next = nextHeader;
			header->size = newSize;
		}
		
	} else {
		// attempt to allocate some new memory
		// if we get some then copy the memory over, release the old block,
		//  and return the pointer to the beginning of the new block of data
		result = mem_Allocate_Data( newSize, fileName, line );
		if( result != NULL ) {
			mem_Release( (void*)( header + 1 ) );
		}
	}

	SetMemoryBlockInfo( (void*)( (uint8_t*)result - sizeof( MemoryBlockHeader ) ), fileName, line );
	return result;
}

static void* shrinkBlock( MemoryBlockHeader* header, size_t newSize, const char* fileName, int line )
{
	assert( header != NULL );
	assert( header->size > newSize );

	// see if there's enough left after the shrink for a new block, if there is
	//  then make it and condense it
	if( ( header->size - newSize ) >= ( sizeof( MemoryBlockHeader ) + MIN_ALLOC_SIZE ) ) {
		MemoryBlockHeader* newHeader = createNewBlock( (void*)( (char*)( header + 1 ) + newSize ),
			header, header->next, header->size - sizeof( MemoryBlockHeader) - newSize,
			fileName, line );
		condenseMemoryBlocks( newHeader, fileName, line );
		testingSetMemory( (void*)( newHeader + 1 ), newHeader->size, 0xAA );
		header->size = newSize;
		SetMemoryBlockInfo( header, fileName, line );
	}

	return (void*)( header + 1 );
}

int mem_Init( size_t totalSize )
{
	memoryBlock.memory = SDL_malloc(totalSize);

	testingSetMemory( memoryBlock.memory, totalSize, 0xFF );

	if( memoryBlock.memory == NULL ) {
		return -1;
	}

	createNewBlock( memoryBlock.memory, NULL, NULL, totalSize - sizeof( MemoryBlockHeader ), __FILE__, __LINE__ );

	return 0;
}

void mem_CleanUp( void )
{
	// invalidates all the pointers
	SDL_free( memoryBlock.memory );
	memoryBlock.memory = NULL;
}

void mem_Log( void )
{
	SDL_Log( "=== Memory Use Log ===" );
	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( header != NULL ) {
		SDL_Log( " Memory header: %p", header );
		if( header->guardValue != GUARD_VALUE ) {
			SDL_Log( " ! Memory was corrupted" );
		} else {
#ifdef LOG_MEMORY_ALLOCATIONS
			SDL_Log( "  File: %s  Line: %i", header->file, header->line );
#endif
			SDL_Log( "  Size: %u", header->size );
			SDL_Log( "  In Use: %s", ( header->flags & IN_USE_FLAG ) ? "YES" : "no" );
		}

		header = header->next;
	}
	SDL_Log( "=== End Memory Use Log ===" );
}

void* mem_Allocate_Data( size_t size, const char* fileName, const int line )
{
	assert( memoryBlock.memory != NULL );

	// we'll just do first fit, if we can't find a spot we'll just return NULL
	char* result = NULL;

	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( ( header != NULL ) && ( ( header->flags & IN_USE_FLAG ) || ( header->size < size ) ) ) {
		header = header->next;
	}

	if( header != NULL ) {
		// found a large enough block that's not in use, split it up and set stuff up
		header->flags |= IN_USE_FLAG;

		result = (char*)header;
		result += sizeof( MemoryBlockHeader );

		testingSetMemory( (void*)result, size, 0xCC );

		// if there's enough room left then split it into it's own block

		// how do we really want to do this?
		if( header->size >= ( size + sizeof( MemoryBlockHeader ) + MIN_ALLOC_SIZE ) ) {
			MemoryBlockHeader* nextHeader = createNewBlock( (void*)( result + size ),
				header, header->next, header->size - size - sizeof( MemoryBlockHeader ),
				fileName, line );
			testingSetMemory( (void*)( nextHeader + 1 ), nextHeader->size, 0xDD );
		} else {
			// there's some left over memory, we'll just put it into the block
			testingSetMemory( (void*)( result + size ), header->size - size, 0xEE );
			size = header->size;
		}

		header->size = size;
	}

	assert( result != NULL );
	return (void*)result;
}

void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line )
{
	assert( memoryBlock.memory != NULL );

	if( newSize == 0 ) {
		mem_Release( memory );
		return NULL;
	}

	// two cases, when we want more and when we want less
	// we'll see if there's enough memory in the next block, if there is then just
	//  expand this block, otherwise find a space to fit it
	// if we can't find a space then we'll return NULL, but won't deallocate the old
	//  memory
	// or we could just assert
	void* result = memory;

	if( memory != NULL ) {
		MemoryBlockHeader* header = (MemoryBlockHeader*)( ( (char*)memory ) - sizeof( MemoryBlockHeader ) );
		if( newSize > header->size ) {
			result = growBlock( header, newSize, fileName, line );
		} else if( newSize < header->size ) {
			result = shrinkBlock( header, newSize, fileName, line );
		}
	} else {
		result = mem_Allocate( newSize );
	}

	assert( result != NULL );
	return result;
}

void mem_Release_Data( void* memory, const char* fileName, const int line )
{
	assert( memoryBlock.memory != NULL );

	if( memory == NULL ) {
		return;
	}
	
	// set the associated block as not in use, and merge with nearby blocks if they're
	//  not in use
	MemoryBlockHeader* header = (MemoryBlockHeader*)( ((char*)memory) - sizeof( MemoryBlockHeader ) );
	header->flags &= ~IN_USE_FLAG;

	assert( header->guardValue == GUARD_VALUE );
	
	header = condenseMemoryBlocks( header, fileName, line );
	testingSetMemory( (void*)( ( (char*)header ) + sizeof( MemoryBlockHeader ) ), header->size, 0xFF );
}