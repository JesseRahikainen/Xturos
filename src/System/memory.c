#include "memory.h"

#include <SDL.h>
#include <stdint.h>

#define GUARD_VALUE 0xDEADBEEF
#define MIN_ALLOC_SIZE 16

#define IN_USE_FLAG ( 1 << 31 )

#define TEST_CLEAR_VALUES 1

// TODO: Get aligned memory allocation working.

// TODO: we can probably assume that there will never be a two unused blocks next to each other, unless we want some way to
//   reserve memory in the future, in which case we don't want to make that assumption

typedef struct MemoryBlockHeader {
	uint32_t guardValue;

	struct MemoryBlockHeader* next;
	struct MemoryBlockHeader* prev;

	uint32_t flags;
	size_t size;
} MemoryBlockHeader;

// TODO: have all the functions take in a Memory structure so we can do something like memory pools. But how to do that without
//   breaking having signatures similar to the standard c library memory allocation functions?
typedef struct {
	void* memory;
} Memory;

static Memory memoryBlock;

#if TEST_CLEAR_VALUES == 1
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

// returns the remaining block after the condensation
static MemoryBlockHeader* condenseMemoryBlocks( MemoryBlockHeader* start )
{
	SDL_assert( start != NULL );
	SDL_assert( !( start->flags & IN_USE_FLAG ) );

	// find the earliest block that's not in use
	while( ( start->prev != NULL ) && !( start->prev->flags & IN_USE_FLAG ) ) {
		start = start->prev;
	}

	// going to the right, merge unused blocks
	while( ( start->next != NULL ) && !( start->next->flags & IN_USE_FLAG ) ) {
		MemoryBlockHeader* nextHeader = start->next;
		start->size += nextHeader->size + sizeof( MemoryBlockHeader );
		start->next = nextHeader->next;
		if( start->next != NULL ) {
			start->next->prev = start;
		}
	}

	return start;
}

static MemoryBlockHeader* createNewBlock( void* start, MemoryBlockHeader* prev, MemoryBlockHeader* next, size_t size )
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

	return header;
}

static void* growBlock( MemoryBlockHeader* header, size_t newSize )
{
	SDL_assert( header != NULL );
	SDL_assert( header->size < newSize );

	void* result = NULL;

	// two cases, one where there's enough room to just expand it, the other
	//  where we'll have to release the current and allocate a new position
	//  for it

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
			MemoryBlockHeader* nextHeader = createNewBlock( (void*)( (char*)result + newSize ),
				header, header->next, header->size - newSize - sizeof( MemoryBlockHeader ) );
			testingSetMemory( (void*)( nextHeader + 1 ), nextHeader->size, 0xAA );
			header->size = newSize;
		}
		testingSetMemory( (void*)result, header->size, 0xCC );
	} else {
		// attempt to allocate some new memory
		// if we get some then copy the memory over, release the old block,
		//  and return the pointer to the beginning of the new block of data
		result = mem_allocate( newSize );
		if( result != NULL ) {
			mem_release( (void*)( header + 1 ) );
		}
	}

	return result;
}

static void* shrinkBlock( MemoryBlockHeader* header, size_t newSize )
{
	SDL_assert( header != NULL );
	SDL_assert( header->size > newSize );

	// see if there's enough left after the shrink for a new block, if there is
	//  then make it and condense it
	if( ( header->size - newSize ) >= ( sizeof( MemoryBlockHeader ) + MIN_ALLOC_SIZE ) ) {
		MemoryBlockHeader* newHeader = createNewBlock( (void*)( (char*)( header + 1 ) + newSize ),
			header, header->next, header->size - sizeof( MemoryBlockHeader) - newSize );
		condenseMemoryBlocks( newHeader );
		testingSetMemory( (void*)( newHeader + 1 ), newHeader->size, 0xAA );
		header->size = newSize;
	}

	return (void*)( header + 1 );
}

int initMemory( size_t totalSize )
{
	memoryBlock.memory = SDL_malloc(totalSize);

	testingSetMemory( memoryBlock.memory, totalSize, 0xFF );

	if( memoryBlock.memory == NULL ) {
		return -1;
	}

	createNewBlock( memoryBlock.memory, NULL, NULL, totalSize - sizeof( MemoryBlockHeader ) );

	return 0;
}

void cleanUpMemory( void )
{
	// invalidates all the pointers
	SDL_free( memoryBlock.memory );
	memoryBlock.memory = NULL;
}

void* mem_allocate( size_t size )
{
	SDL_assert( memoryBlock.memory != NULL );

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
				header, header->next, header->size - size - sizeof( MemoryBlockHeader ) );
			testingSetMemory( (void*)( nextHeader + 1 ), nextHeader->size, 0xDD );
		} else {
			// there's some left over memory, we'll just put it into the block
			testingSetMemory( (void*)( result + size ), header->size - size, 0xEE );
			size = header->size;
		}

		header->size = size;
	}

	SDL_assert( result != NULL );
	return (void*)result;
}

void* mem_resize( void* memory, size_t newSize )
{
	SDL_assert( memoryBlock.memory != NULL );

	if( newSize == 0 ) {
		mem_release( memory );
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
			result = growBlock( header, newSize );
		} else if( newSize < header->size ) {
			result = shrinkBlock( header, newSize );
		}
	} else {
		result = mem_allocate( newSize );
	}

	SDL_assert( result != NULL );
	return result;
}

void mem_release( void* memory )
{
	SDL_assert( memoryBlock.memory != NULL );
	SDL_assert( memory != NULL );
	
	// set the associated block as not in use, and merge with nearby blocks if they're
	//  not in use
	MemoryBlockHeader* header = (MemoryBlockHeader*)( ((char*)memory) - sizeof( MemoryBlockHeader ) );
	header->flags &= ~IN_USE_FLAG;

	SDL_assert( header->guardValue == GUARD_VALUE );
	
	header = condenseMemoryBlocks( header );
	testingSetMemory( (void*)( ( (char*)header ) + sizeof( MemoryBlockHeader ) ), header->size, 0xFF );
}