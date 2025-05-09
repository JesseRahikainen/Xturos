#include "memory.h"

// we use assert here instead of SDL_assert since SDL_assert will allocate memory, which can lead to a stack overflow if we detect an issue in the memory
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_mutex.h>
#include <stdbool.h>

#include "platformLog.h"
#include "Utils/helpers.h"

/*
If we know the initial address is aligned, and the address for the data is aligned, and the size is aligned
 then the data after the block should also be aligned, so we can just assume that will work
If we assume the alignment is n, then all aligned addresses are in some value m*n, given we have the values that
 we know are aligned
 headerAddr = h*n
 dataStart = d*n
 dataSize = s*n

 then the address of the next header is at (d*n)+(s*n), which is (d+s)*n and as such should be aligned
So as long as we make sure everything is aligned to begin with then everything should be aligned in the end
We can also assume the initial header is memory aligned as it's address is gotten from the system memory allocation

Also, given the memory alignment and the size of the header we should be able to know how far back we have to
jump for the delete. Just need to align the size of the header.
*/

#if defined( __ANDROID__ )
	// from allocators.cpp in android-ndk-r13b
	//  would be nicer if the platforms had some sort of ALIGNMENT definition that we could access
	#if defined (__OS400__)
		#define ALIGN 16
	#else
		#define ALIGN ( sizeof(void*) * 2 )
	#endif
#else
	#define ALIGN 64
#endif

#define GUARD_VALUE 0xDEADBEEF
#define MIN_ALLOC_SIZE ( ALIGN ) // this needs to at least fit the alignment in bytes

#define IN_USE_FLAG ( 1 << 31 )

#if defined(_DEBUG)
// debug flags
//#define TEST_CLEAR_VALUES
#define LOG_MEMORY_ALLOCATIONS
#define TEST_EVERY_CHANGE
#endif

typedef struct MemoryBlockHeader {
	uint32_t guardValue;

	struct MemoryBlockHeader* next;
	struct MemoryBlockHeader* prev;

	uint32_t flags;
	size_t size; // the amount of memory this block stores

	// hierarchical pointers
	struct MemoryBlockHeader* parent;
	struct MemoryBlockHeader* firstChild;
	struct MemoryBlockHeader* nextSibling;

	// last file and line in that file that modified this block
#ifdef LOG_MEMORY_ALLOCATIONS
	char file[256];
	int line;
	char note[32];
#endif

	uint32_t postGuardValue;
} MemoryBlockHeader;

// TODO: have all the functions take in a Memory structure so we can do something like memory pools. But how to do that without
//   breaking having signatures similar to the standard c library memory allocation functions?
// Could have an external pool used by everything that isn't in the engine, then a use other pools for in engine stuff
// Or have special versions that access certain pools, so we'd have something like mem_Allocate_Spine( size_t s ) that
//  would call mem_Allocate with spineMemory. This would involve creating a lot of memory pools though.
typedef struct {
	void* memory;
	SDL_Mutex* mutex;

	void* watchedAddress;
	MemoryBlockHeader* watchedHeader;
	size_t totalSize;
} MemoryArena;

static MemoryArena memoryBlock = { NULL, NULL, NULL, NULL };

#ifdef THREAD_SUPPORT
static void lockMemoryMutex( void )
{
	if( memoryBlock.mutex == NULL ) return;
	SDL_LockMutex( memoryBlock.mutex );
}

static void unlockMemoryMutex( void )
{
	if( memoryBlock.mutex == NULL ) return;
	SDL_UnlockMutex( memoryBlock.mutex );
}
#else
static void lockMemoryMutex( void ) { }
static void unlockMemoryMutex( void ) { }
#endif

static void* watchedAddress = NULL;
static MemoryBlockHeader* watchedHeader = NULL;

static void* alignAddress( void* addr )
{
	return (void*)( ( ( (uintptr_t)addr + ( ALIGN - 1 ) ) / ALIGN ) * ALIGN );
}

// makes it so the passed in size matches the desired alignment
#define ALIGN_SIZE( s ) ( ( ( ( s ) + ( ALIGN - 1 ) ) / ALIGN ) * ALIGN )

#define MEMORY_HEADER_SIZE ( ALIGN_SIZE( sizeof( MemoryBlockHeader ) ) )

static MemoryBlockHeader* findMemoryBlock( void* ptr, bool ensureInUse )
{
	MemoryBlockHeader* block = NULL;

	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( header != NULL ) {
		if( !ensureInUse || ( header->flags & IN_USE_FLAG ) ) {
			void* dataStart = (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE );
			void* dataEnd = (void*)( (uint8_t*)( dataStart ) + header->size );
			if( ( ptr >= dataStart ) && ( ptr < dataEnd ) ) {
				return header;
			}
		}
		header = header->next;
	}

	return NULL;
}

#include <inttypes.h>
static void memoryBlockLogDump( MemoryBlockHeader* header )
{
	if( header != NULL ) {
		llog( LOG_DEBUG, " Memory header: 0x%p", header );
		if( header->guardValue != GUARD_VALUE ) {
			llog( LOG_DEBUG, " ! Memory was corrupted from beginning" );
		} else if( header->postGuardValue != GUARD_VALUE ) {
			llog( LOG_DEBUG, " ! Memory was corrupted from end" );
		} else {
#ifdef LOG_MEMORY_ALLOCATIONS
			llog( LOG_DEBUG, "  File: %s", header->file );
			llog( LOG_DEBUG, "   Line: %i", header->line );
			llog( LOG_DEBUG, "   Note: %s", header->note );
#endif
			llog( LOG_DEBUG, "  Size: %u", header->size );
			llog( LOG_DEBUG, "  In Use: %s", ( header->flags & IN_USE_FLAG ) ? "YES" : "no" );
			llog( LOG_DEBUG, "  Start addr: 0x" PRIxPTR "p", (uintptr_t)header + MEMORY_HEADER_SIZE );
			llog( LOG_DEBUG, "  End addr: 0x" PRIxPTR "p", (uintptr_t)header + MEMORY_HEADER_SIZE + header->size );
		}
	} else {
		llog( LOG_DEBUG, "  NULL header!" );
	}
}

#ifdef TEST_CLEAR_VALUES
static void testingSetMemory( void* start, size_t size, uint8_t val )
{
	/*uint8_t* data = (uint8_t*)start;
	for( size_t i = 0; i < size; ++i ) {
		data[i] = val;
	}*/
	memset( start, val, size );
}
#else
static void testingSetMemory( void* start, size_t size, uint8_t val ) { }
#endif

// assume header is never NULL
#ifdef LOG_MEMORY_ALLOCATIONS
static void setMemoryBlockInfo( MemoryBlockHeader* header, const char* fileName, int line, const char* note )
{
	int lastFileIdx = ( sizeof( header->file ) / sizeof( header->file[0] ) ) - 1;
	strncpy( header->file, fileName, lastFileIdx );
	header->file[lastFileIdx] = 0;
	header->line = line;

	int lastNoteIdx = ( sizeof( header->note ) / sizeof( header->note[0] ) ) - 1;
	strncpy( header->note, note, lastNoteIdx );
	header->note[lastNoteIdx] = 0;
}
#else
static void setMemoryBlockInfo( MemoryBlockHeader* header, const char* fileName, int line, const char* note ) { }
#endif

#ifdef LOG_WATCHED_MEMORY_ADDRESS_CHANGES
#define BETWEEN( v, min, max ) ( ( v >= min ) && ( v <= max ) )
static void logWatchedMemoryAddressChange( MemoryBlockHeader* adjHeader, const char* message )
{
	if( watchedHeader == NULL ) return;
	if( adjHeader == NULL ) return;

	// see if the adjusted memory block header, and the watched memory block overlap at all
	void* watchedStart = (void*)watchedHeader;
	void* watchedEnd = (void*)( ( (uint8_t*)watchedHeader ) + MEMORY_HEADER_SIZE + watchedHeader->size );

	void* adjStart = (void*)adjHeader;
	void* adjEnd = (void*)( ( (uint8_t*)adjHeader ) + MEMORY_HEADER_SIZE + adjHeader->size );

	if( BETWEEN( watchedStart, adjStart, adjEnd ) ||
		BETWEEN( watchedEnd, adjStart, adjEnd ) ||
		BETWEEN( adjStart, watchedStart, watchedEnd ) ||
		BETWEEN( adjEnd, watchedStart, watchedEnd ) ) {

		llog( LOG_DEBUG, "=== Address Change: %p ===", watchedAddress );
		if( message != NULL ) llog( LOG_DEBUG, " %s", message );
		memoryBlockLogDump( watchedHeader );
		llog( LOG_DEBUG, "=== End Address change ===" );
	}	
}
#else
static void logWatchedMemoryAddressChange( MemoryBlockHeader* header, void* ptr, const char* message ) { }
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
		setMemoryBlockInfo( start, fileName, line, "Condense" );
		MemoryBlockHeader* nextHeader = start->next;
		start->size += nextHeader->size + MEMORY_HEADER_SIZE;
		start->next = nextHeader->next;
		if( start->next != NULL ) {
			start->next->prev = start;
		}
	}

	return start;
}

void static internal_DetachFromParent( MemoryBlockHeader* childHeader )
{
	if( childHeader->parent == NULL ) return;

	// remove from linked list in parent
	MemoryBlockHeader* siblingHeader = childHeader->parent->firstChild;

	// if this is the first child the parent should point to the next sibling instead
	if( childHeader->parent->firstChild == childHeader ) {
		childHeader->parent->firstChild = childHeader->nextSibling;
	}

	while( siblingHeader != NULL ) {
		if( siblingHeader->nextSibling == childHeader ) {
			siblingHeader->nextSibling = childHeader->nextSibling;
			break;
		}
		siblingHeader = siblingHeader->nextSibling;
	}

	childHeader->nextSibling = NULL;
	childHeader->parent = NULL;
}

static MemoryBlockHeader* createNewBlock( void* start, MemoryBlockHeader* prev, MemoryBlockHeader* next, size_t size, const char* fileName, int line )
{
	MemoryBlockHeader* header = (MemoryBlockHeader*)start;

	header->guardValue = GUARD_VALUE;
	header->postGuardValue = GUARD_VALUE;
	header->flags = 0;
	header->size = size;

	header->parent = NULL;
	header->firstChild = NULL;
	header->nextSibling = NULL;

	if( prev != NULL ) {
		prev->next = header;
	}
	header->prev = prev;

	if( next != NULL ) {
		next->prev = header;
	}
	header->next = next;

	setMemoryBlockInfo( header, fileName, line, "Create" );

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
		sizeAllowed += scan->size + MEMORY_HEADER_SIZE;
		scan = scan->next;
	}

	if( newSize < sizeAllowed ) {
		// claim all the next headers
		result = (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE );
		scan = header->next;
		while( ( scan != NULL ) && !( scan->flags & IN_USE_FLAG ) ) {
			// unlink the scan block
			if( scan->next != NULL ) scan->next->prev = scan->prev;
			/*if( scan->prev != NULL ) scan->prev->next = scan->next; */
			header->next = scan->next;

			// claim the memory
			header->size += scan->size + MEMORY_HEADER_SIZE;

			scan = scan->next;
		}

		// see if there's enough left over to create a new block
		if( header->size >= ( newSize + MEMORY_HEADER_SIZE + MIN_ALLOC_SIZE ) ) {
			MemoryBlockHeader* nextHeader = createNewBlock( (void*)( (uint8_t*)result + newSize ),
				header, scan, header->size - newSize - MEMORY_HEADER_SIZE,
				fileName, line );
			header->next = nextHeader;
			header->size = newSize;
		}
		
	} else {
		// attempt to allocate some new memory
		// if we get some then copy the memory over, release the old block,
		//  and return the pointer to the beginning of the new block of data
		//  if the allocation fails the old memory isn't cleaned up and this will return NULL
		result = mem_Allocate_Data( newSize, fileName, line );
		if( result != NULL ) {
			// adjust the parent/child pointers, general use case is no parents or children, so make those fastest
			if( header->parent != NULL ) {
				MemoryBlockHeader* newBlockHeader = findMemoryBlock( result, true );

				// point new header to the old parent
				newBlockHeader->parent = header->parent;
				
				// adjust sibling pointers
				newBlockHeader->nextSibling = header->nextSibling;
				MemoryBlockHeader* sibling = header->parent->firstChild;
				while( sibling != NULL ) {
					if( sibling->nextSibling == header ) {
						sibling->nextSibling = newBlockHeader;
						break;
					}
				}

				if( newBlockHeader->parent->firstChild == header ) {
					newBlockHeader->parent->firstChild = newBlockHeader;
				}
			}

			if( header->firstChild != NULL ) {
				// change the parent pointer of all the children and the first child of the new header
				MemoryBlockHeader* newBlockHeader = findMemoryBlock( result, true );

				newBlockHeader->firstChild = header->firstChild;

				MemoryBlockHeader* child = newBlockHeader->firstChild;
				while( child != NULL ) {
					child->parent = newBlockHeader;
					child = child->nextSibling;
				}
			}

			memcpy( result, (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE ), header->size );
			mem_Release( (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE ) );
		}
	}

	if( result != NULL ) {
		logWatchedMemoryAddressChange( (MemoryBlockHeader*)( (uint8_t*)result - MEMORY_HEADER_SIZE ), "growBlock", NULL );
		setMemoryBlockInfo( (MemoryBlockHeader*)( (uint8_t*)result - MEMORY_HEADER_SIZE ), fileName, line, "Grow" );
	}
	return result;
}

static void* shrinkBlock( MemoryBlockHeader* header, size_t newSize, const char* fileName, int line )
{
	assert( header != NULL );
	assert( header->size > newSize );

	// see if there's enough left after the shrink for a new block, if there is
	//  then make it and condense it
	if( ( header->size - newSize ) >= ( MEMORY_HEADER_SIZE + MIN_ALLOC_SIZE ) ) {
		MemoryBlockHeader* newHeader = createNewBlock( (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE + newSize ),
			header, header->next, header->size - MEMORY_HEADER_SIZE - newSize,
			fileName, line );
		condenseMemoryBlocks( newHeader, fileName, line );
		testingSetMemory( (void*)( (uintptr_t)newHeader + MEMORY_HEADER_SIZE ), newHeader->size, 0xAA );
		header->size = newSize;
		setMemoryBlockInfo( header, fileName, line, "Shrink" );
	}

	logWatchedMemoryAddressChange( header, "shrinkBlock", NULL );
	return (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE );
}

static void internal_log( void )
{
	llog( LOG_DEBUG, "=== Memory Use Log ===" );
	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( header != NULL ) {
		memoryBlockLogDump( header );
		header = header->next;
	}
	llog( LOG_DEBUG, "=== End Memory Use Log ===" );
}

static void internal_logAddressBlockData( void* ptr, const char* extra )
{
	// first find the memory block that the pointer is in
	MemoryBlockHeader* header = findMemoryBlock( ptr, false );
	llog( LOG_DEBUG, "=== Pointer Block Data %p ===", ptr );
	if( extra != NULL ) llog( LOG_DEBUG, " %s", extra );
	memoryBlockLogDump( header );
	llog( LOG_DEBUG, "=== End Pointer Block Data ===" );
}

static void internal_verify( void )
{
	// just follow the list, verifying that the guard value is correct
	//  also make sure all the previous and next pointers are correct
	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	bool firstBlock = true;
	while( header != NULL ) {
		assert( header->guardValue == GUARD_VALUE );
		assert( header->postGuardValue == GUARD_VALUE );

		if( header->prev != NULL ) {
			assert( header->prev->next == header );
		} else {
			assert( firstBlock );
		}

		if( header->next != NULL ) {
			assert( header->next->prev == header );
		}

		header = header->next;
		firstBlock = false;
	}
}

static bool internal_getVerify( void )
{
	// just follow the list, verifying that the guard value is correct
	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( header != NULL ) {
		if( header->guardValue != GUARD_VALUE ) {
			return false;
		}
		if( header->postGuardValue != GUARD_VALUE ) {
			return false;
		}
		header = header->next;
	}

	return true;
}

static void internal_verifyPointer( void* p, bool allowNull )
{
	if( allowNull && p == NULL ) return;

	// verify the pointer is pointing to valid memory
	MemoryBlockHeader* foundHeader = findMemoryBlock( p, true );
	if( foundHeader == NULL ) {
		int x = 0;
	}
	assert( foundHeader != NULL );
}

static void internal_getReportValues( size_t* totalOut, size_t* inUseOut, size_t* overheadOut, uint32_t* fragmentsOut )
{
	size_t total = 0;
	size_t inUse = 0;
	size_t overhead = 0;
	uint32_t fragments = 0; // blocks not in use

	MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
	while( header != NULL ) {
		if( header->flags & IN_USE_FLAG ) {
			inUse += header->size;
		} else {
			++fragments;
		}
		overhead += MEMORY_HEADER_SIZE;
		total += ( header->size + MEMORY_HEADER_SIZE );

		header = header->next;
	}

	if( totalOut != NULL ) (*totalOut) = total;
	if( inUseOut != NULL ) (*inUseOut) = inUse;
	if( overheadOut != NULL ) (*overheadOut) = overhead;
	if( fragmentsOut != NULL ) (*fragmentsOut) = fragments;
}

static void internal_report( void )
{
	size_t total = 0;
	size_t inUse = 0;
	size_t overhead = 0;
	uint32_t fragments = 0; // blocks not in use

	internal_getReportValues( &total, &inUse, &overhead, &fragments );

	// TODO: Find out why %zu doesn't work...
	llog( LOG_DEBUG, "Memory Report:" );
	llog( LOG_DEBUG, "  Total: %u", total );
	llog( LOG_DEBUG, "  In Use: %u", inUse );
	llog( LOG_DEBUG, "  Overhead: %u", overhead );
	llog( LOG_DEBUG, "  Fragments: %u", fragments );
}

static void internal_release_Data( void* memory, const char* fileName, const int line )
{
#ifdef TEST_EVERY_CHANGE
	mem_Verify( );
	internal_verifyPointer( memory, true );
#endif
	ASSERT_AND_IF_NOT( memoryBlock.memory != NULL ) return;

	if( memory == NULL ) return;

	void* memoryStart = memoryBlock.memory;
	void* memoryEnd = (void*)( (uint8_t*)memory + ( 256 * 1024 * 1024 ) );

	MemoryBlockHeader* header = (MemoryBlockHeader*)( (uintptr_t)memory - MEMORY_HEADER_SIZE );
	MemoryBlockHeader* startHeader = header;

	// release child blocks as well
	if( header->firstChild != NULL ) {
		MemoryBlockHeader* child = header->firstChild;
		while( child != NULL ) {
			MemoryBlockHeader* next = child->nextSibling;
			void* childMemory = (void*)( (uintptr_t)child + MEMORY_HEADER_SIZE );
			internal_release_Data( childMemory, fileName, line );
			child = next;
		}
	}

	// detach from parent block
	internal_DetachFromParent( header );

	// set the associated block as not in use, and merge with nearby blocks if they're
	//  not in use
	logWatchedMemoryAddressChange( header, "mem_Release_Data", NULL );
	header->flags &= ~IN_USE_FLAG;

	assert( header->guardValue == GUARD_VALUE );
	assert( header->postGuardValue == GUARD_VALUE );
	
	header = condenseMemoryBlocks( header, fileName, line );
	testingSetMemory( (void*)( (uintptr_t)header + MEMORY_HEADER_SIZE ), header->size, 0xAB );

#ifdef TEST_EVERY_CHANGE
	mem_Verify( );
#endif
}

static void internal_watchAddress( void* ptr )
{
	watchedAddress = ptr;
	watchedHeader = findMemoryBlock( ptr, false );
	llog( LOG_DEBUG, "=== Start Watching Memory Address: %p ===", ptr );
	memoryBlockLogDump( watchedHeader );
	llog( LOG_DEBUG, "=== End Start Watching Memory Address ===" );
}

static void internal_unwatchAddress( void* ptr )
{
	if( watchedAddress == ptr ) {
		llog( LOG_DEBUG, "=== Stop Watching Memory Address: %p ===", ptr );
		memoryBlockLogDump( watchedHeader );
		llog( LOG_DEBUG, "=== End Stop Watching Memory Address ===" );

		watchedAddress = NULL;
		watchedHeader = NULL;
	}
}

bool mem_Init( size_t totalSize )
{
	memoryBlock.mutex = NULL;
	memoryBlock.watchedAddress = NULL;
	memoryBlock.watchedHeader = NULL;

	memoryBlock.memory = malloc(totalSize);
	if( memoryBlock.memory == NULL ) {
		//llog( LOG_CRITICAL, "Error allocating memory." ); // the logging won't be enabled when this is called
		goto error_cleanup;
	}
	memoryBlock.totalSize = totalSize;

	testingSetMemory( memoryBlock.memory, totalSize, 0xFF );

	createNewBlock( memoryBlock.memory, NULL, NULL, totalSize - MEMORY_HEADER_SIZE, __FILE__, __LINE__ );

	SDL_SetMemoryFunctions( mem_AllocateForCallback, mem_ClearAllocateForCallback, mem_ResizeForCallback, mem_ReleaseForCallback );

#ifdef THREAD_SUPPORT
	memoryBlock.mutex = SDL_CreateMutex( );
	if( memoryBlock.mutex == NULL ) {
		//llog( LOG_CRITICAL, "Unable to create memory mutex: %s", SDL_GetError( ) ); // the logging won't be enabled when this is called
		goto error_cleanup;
	}
#endif

	return true;

error_cleanup:
	mem_CleanUp( );
	return false;
}

void mem_CleanUp( void )
{
#ifdef THREAD_SUPPORT
	SDL_DestroyMutex( memoryBlock.mutex );
#endif

	// invalidates all the pointers
	free( memoryBlock.memory );
	memoryBlock.memory = NULL;
}

void mem_Log( void )
{
	lockMemoryMutex( ); {
		internal_log( );
	} unlockMemoryMutex( );
}

void mem_LogAddressBlockData( void* ptr, const char* extra )
{
	lockMemoryMutex( ); {
		internal_logAddressBlockData( ptr, extra );
	} unlockMemoryMutex( );
}

void mem_Verify( void )
{
	lockMemoryMutex( ); {
		internal_verify( );
	} unlockMemoryMutex( );
}

bool mem_GetVerify( void )
{
	bool ret;
	lockMemoryMutex( ); {
		ret = internal_getVerify( );
	} unlockMemoryMutex( );
	return ret;
}

void mem_VerifyPointer( void* p, bool allowNull )
{
	lockMemoryMutex( ); {
		internal_verifyPointer( p, allowNull );
	} unlockMemoryMutex( );
}

void mem_Report( void )
{
	lockMemoryMutex( ); {
		internal_report( );
	} unlockMemoryMutex( );
}

void mem_GetReportValues( size_t* totalOut, size_t* inUseOut, size_t* overheadOut, uint32_t* fragmentsOut )
{
	lockMemoryMutex( ); {
		internal_getReportValues( totalOut, inUseOut, overheadOut, fragmentsOut );
	} unlockMemoryMutex( );
}

void* mem_Allocate_Data( size_t size, const char* fileName, const int line )
{
	// if the size is 0 malloc can return NULL or an unusable pointer, NULL works better for us as
	//  it avoids littering the memory with zero sized headers
	if( size == 0 ) return NULL;

	uint8_t* result = NULL;

	lockMemoryMutex( ); {
#ifdef TEST_EVERY_CHANGE
		internal_verify( );
#endif
		assert( memoryBlock.memory != NULL );

		size = ALIGN_SIZE( size );

		// we'll just do first fit, if we can't find a spot we'll just return NULL
		MemoryBlockHeader* header = (MemoryBlockHeader*)( memoryBlock.memory );
		while( ( header != NULL ) && ( ( header->flags & IN_USE_FLAG ) || ( header->size < size ) ) ) {
			header = header->next;
		}

		if( header != NULL ) {
			// found a large enough block that's not in use, split it up and set stuff up
			header->flags |= IN_USE_FLAG;

			result = (uint8_t*)header;
			result += MEMORY_HEADER_SIZE;

			testingSetMemory( (void*)result, size, 0xCC );

			// if there's enough room left then split it into it's own block

			// how do we really want to do this?
			if( header->size >= ( size + MEMORY_HEADER_SIZE + MIN_ALLOC_SIZE ) ) {
				MemoryBlockHeader* nextHeader = createNewBlock( (void*)( result + size ),
					header, header->next, header->size - size - MEMORY_HEADER_SIZE,
					fileName, line );
				testingSetMemory( (void*)( (uintptr_t)nextHeader + MEMORY_HEADER_SIZE ), nextHeader->size, 0xDD );
			} else {
				// there's some left over memory, we'll just put it into the block
				testingSetMemory( (void*)( result + size ), header->size - size, 0xEE );
				size = header->size;
			}

			header->size = size;
			setMemoryBlockInfo( header, fileName, line, "Allocate" );
		}
	#ifdef TEST_EVERY_CHANGE
		mem_Verify( );
	#endif
		assert( result != NULL );

		if( result != NULL ) {
	#ifdef TEST_EVERY_CHANGE
			internal_verifyPointer( result, false );
	#endif
			logWatchedMemoryAddressChange( (MemoryBlockHeader*)( (uint8_t*)result - MEMORY_HEADER_SIZE ), "mem_Allocate_Data", NULL );
		}

	} unlockMemoryMutex( );

	return (void*)result;
}

void* mem_Resize_Data( void* memory, size_t newSize, const char* fileName, const int line )
{
	void* result = memory;
	lockMemoryMutex( ); {
#ifdef TEST_EVERY_CHANGE
		internal_verify( );
		internal_verifyPointer( result, true );
#endif
		assert( memoryBlock.memory != NULL );

		if( newSize == 0 ) {
			internal_release_Data( memory, fileName, line );
			result = NULL;
		} else {

			newSize = ALIGN_SIZE( newSize );

			// two cases, when we want more and when we want less
			// we'll see if there's enough memory in the next block, if there is then just
			//  expand this block, otherwise find a space to fit it
			// if we can't find a space then we'll return NULL, but won't deallocate the old
			//  memory
			// or we could just assert
			if( memory != NULL ) {
				MemoryBlockHeader* header = (MemoryBlockHeader*)( (uintptr_t)memory - MEMORY_HEADER_SIZE );
				if( newSize > header->size ) {
					result = growBlock( header, newSize, fileName, line );
				} else if( newSize < header->size ) {
					result = shrinkBlock( header, newSize, fileName, line );
				}
			} else {
				result = mem_Allocate_Data( newSize, fileName, line );
			}

#ifdef TEST_EVERY_CHANGE
			mem_Verify( );
#endif
			assert( result != NULL );

			if( result != NULL ) {
#ifdef TEST_EVERY_CHANGE
				internal_verifyPointer( result, false );
#endif
				logWatchedMemoryAddressChange( (MemoryBlockHeader*)( (uintptr_t)result - MEMORY_HEADER_SIZE ), "mem_Resize_Data", NULL );
			}
		}
	} unlockMemoryMutex( );

	return result;
}

void mem_Release_Data( void* memory, const char* fileName, const int line )
{
	lockMemoryMutex( ); {
		internal_release_Data( memory, fileName, line );
	} unlockMemoryMutex( );
}

void mem_WatchAddress( void* ptr )
{
	lockMemoryMutex( ); {
		internal_watchAddress( ptr );
	} unlockMemoryMutex( );
}


void mem_UnWatchAddress( void* ptr )
{
	lockMemoryMutex( ); {
		internal_unwatchAddress( ptr );
	} unlockMemoryMutex( );
}

bool mem_IsAllocatedMemory( void* ptr )
{
	bool isAllocated = false;
	lockMemoryMutex( ); {
		isAllocated = ( ptr != NULL ) && findMemoryBlock( ptr, true ) != NULL;
	} unlockMemoryMutex( );
	return isAllocated;
}

bool mem_Attach( void* parent, void* child )
{
	bool success = false;
	lockMemoryMutex( ); {
		MemoryBlockHeader* parentHeader = findMemoryBlock( parent, true );
		MemoryBlockHeader* childHeader = findMemoryBlock( child, true );

		ASSERT_AND_IF_NOT( parentHeader != NULL ) return false;
		ASSERT_AND_IF_NOT( childHeader != NULL ) return false;

		// detach before attempting to reattach
		if( childHeader->parent != NULL ) {
			internal_DetachFromParent( childHeader );
		}

		// make sure the parent isn't a descendant of child, make sure this remains a tree
		MemoryBlockHeader* searchHeader = parentHeader;
		bool isDescendant = false;
		while( searchHeader != NULL && !isDescendant) {
			if( searchHeader->parent == childHeader ) {
				isDescendant = true;
			} else {
				searchHeader = searchHeader->parent;
			}
		}

		if( !isDescendant ) {
			success = true;
			childHeader->parent = parentHeader;
			childHeader->nextSibling = NULL;

			// set as sibling in chain
			if( parentHeader->firstChild == NULL ) {
				parentHeader->firstChild = childHeader;
			} else {
				MemoryBlockHeader* childChain = parentHeader->firstChild;
				while( childChain->nextSibling != NULL ) {
					childChain = childChain->nextSibling;
				}
				childChain->nextSibling = childHeader;
			}
		}
	} unlockMemoryMutex( );
	return success;
}

void mem_DetachFromParent( void* child )
{
	lockMemoryMutex( ); {
		MemoryBlockHeader* childHeader = findMemoryBlock( child, true );
		ASSERT_AND_IF_NOT( childHeader != NULL ) return;
		internal_DetachFromParent( childHeader );
	} unlockMemoryMutex( );
}

void mem_DetachAllChildren( void* parent )
{
	lockMemoryMutex( ); {
		MemoryBlockHeader* parentHeader = findMemoryBlock( parent, true );
		MemoryBlockHeader* childHeader = parentHeader->firstChild;

		while( childHeader != NULL ) {
			MemoryBlockHeader* nextChild = childHeader->nextSibling;
			childHeader->parent = NULL;
			childHeader->nextSibling = NULL;
			
			childHeader = nextChild;
		}
		parentHeader->firstChild = NULL;
	} unlockMemoryMutex( );
}

static void hierachicalTest( uint32_t detachFlags, uint32_t resizeFlags )
{
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		// set up
		void* pointers[13];
		bool shouldBeReleased[13];
		uint32_t detachFlags = 0; // bits positions are indices that should be detached
		uint32_t resizeFlags = 0; // bits positions are indices that should be increased to 200 bytes

		for( size_t i = 0; i < ARRAY_SIZE( pointers ); ++i ) {
			pointers[i] = mem_Allocate( 100 );
		}
		// junk data for when testing expanding the final pointer
		mem_Allocate( 100 );

		// attach all the children
		for( size_t i = 0; i < ARRAY_SIZE( pointers ); ++i ) {
			shouldBeReleased[i] = true;
			for( size_t a = 0; a < 3; ++a ) {
				size_t childIdx = ( i * 3 ) + a + 1;
				if( childIdx >= ARRAY_SIZE( pointers ) ) continue;
				mem_Attach( pointers[i], pointers[childIdx] );
			}
		}

		// possibly increase size of allocation
		for( size_t i = 0; i < ARRAY_SIZE( pointers ); ++i ) {
			if( ( resizeFlags & ( 1 << i ) ) == 0 ) continue;

			void* newData = mem_Resize( pointers[i], 200 );
			ASSERT_AND_IF( newData != NULL )
			{
				pointers[i] = newData;
			}
		}

		// detach any combination of children, mark them as not deleted
		for( size_t i = 0; i < ARRAY_SIZE( pointers ); ++i ) {
			if( i == 0 ) continue; // root will have no parent
			if( ( detachFlags & ( 1 << i ) ) == 0 ) continue;

			mem_DetachFromParent( pointers[i] );

			// mark all descendants as not deleted
			shouldBeReleased[i] = false;
			for( size_t a = 0; a < 3; ++a ) {
				size_t childIdx = ( i * 3 ) + a + 1;
				if( childIdx >= ARRAY_SIZE( pointers ) ) continue;
				shouldBeReleased[childIdx] = false;
			}
		}

		// delete the root pointer and check if the expected children are deleted
		mem_Release( pointers[0] );
		for( size_t i = 0; i < ARRAY_SIZE( pointers ); ++i ) {
			assert( mem_IsAllocatedMemory( pointers[i] ) == !shouldBeReleased[i] );
		}
	} mem_CleanUp( );
}

void* mem_AllocateForCallback( size_t size )
{
	void* data = mem_Allocate( size );
	return data;
}

void* mem_ClearAllocateForCallback( size_t numMembers, size_t memberSize )
{
	size_t totalSize = numMembers * memberSize;
	void* data = mem_Allocate( totalSize );
	if( data != NULL ) {
		SDL_memset( data, 0, totalSize );
	}
	return data;
}

void* mem_ResizeForCallback( void* memory, size_t size )
{
	void* data = mem_Resize( memory, size );
	return data;
}

void mem_ReleaseForCallback( void* memory )
{
	mem_Release( memory );
}

void mem_RunTests( void )
{
	void* oldMemoryBlock = memoryBlock.memory;

	uint8_t* testOne;
	uint8_t* testTwo;
	uint8_t* testThree;
	uint8_t* backup;

	// test basic allocation and release
	llog( LOG_INFO, "Test basic allocation and release..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		assert( testOne != NULL );
		mem_Verify( );

		mem_Release( testOne );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test multiple allocations and releases
	llog( LOG_INFO, "Test multiple allocations and releases..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		assert( testOne != NULL );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 );
		assert( testTwo != NULL );
		mem_Verify( );

		mem_Release( testTwo );
		mem_Verify( );

		mem_Release( testOne );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test basic resize
	llog( LOG_INFO, "Test basic resize..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 1000 );
		assert( testOne != NULL );
		mem_Verify( );

		testOne = (uint8_t*)mem_Resize( testOne, 500 );
		assert( testOne != NULL );
		mem_Verify( );

		testOne = (uint8_t*)mem_Resize( testOne, 750 );
		assert( testOne != NULL );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test resize grow with enough open space in next block to allocate data, and enough data to create new header
	llog( LOG_INFO, "Test resize grow with enough open space in next block to allocate data, and enough data to create new header..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		assert( testOne != NULL );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 + MEMORY_HEADER_SIZE + MIN_ALLOC_SIZE );
		assert( testTwo != NULL );
		mem_Verify( );

		testThree = (uint8_t*)mem_Allocate( 100 );
		assert( testThree != NULL );
		mem_Verify( );

		mem_Release( testTwo );
		mem_Verify( );

		backup = testOne;
		testOne = mem_Resize( testOne, 200 );
		assert( testOne != NULL );
		assert( testOne == backup );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test resize grow with enough open space in next block to allocate data, and not enough space to create new header
	llog( LOG_INFO, "Test resize grow with enough open space in next block to allocate data, and not enough space to create new header..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		assert( testOne != NULL );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 );
		assert( testTwo != NULL );
		mem_Verify( );

		testThree = (uint8_t*)mem_Allocate( 100 );
		assert( testThree != NULL );
		mem_Verify( );

		mem_Release( testTwo );
		mem_Verify( );

		backup = testOne;
		testOne = mem_Resize( testOne, 199 );
		assert( testOne != NULL );
		assert( testOne == backup );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test resize grow without enough open space in next block to allocate data
	llog( LOG_INFO, "Test resize grow without enough open space in next block to allocate data..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		assert( testOne != NULL );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 );
		assert( testTwo != NULL );
		mem_Verify( );

		testThree = (uint8_t*)mem_Allocate( 100 );
		assert( testThree != NULL );
		mem_Verify( );

		mem_Release( testTwo );
		mem_Verify( );

		backup = testOne;
		testOne = mem_Resize( testOne, 1000 );
		assert( testOne != NULL );
		assert( testOne != backup );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test resize shrink where there is enough open space after to create new block
	llog( LOG_INFO, "Test resize shrink where there is enough open space after to create new block..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 + MEMORY_HEADER_SIZE + MIN_ALLOC_SIZE );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 );
		mem_Verify( );

		backup = testOne;
		testOne = (uint8_t*)mem_Resize( testOne, 10 );
		assert( testOne != NULL );
		assert( testOne == backup );
		mem_Verify( );

	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test resize shrink where there is not enough open space after to create new block
	llog( LOG_INFO, "Test resize shrink where there is not enough open space after to create new block..." );
	assert( mem_Init( 32 * 1024 ) == 0 ); {
		testOne = (uint8_t*)mem_Allocate( 100 );
		mem_Verify( );

		testTwo = (uint8_t*)mem_Allocate( 100 );
		mem_Verify( );

		backup = testOne;
		testOne = (uint8_t*)mem_Resize( testOne, 99 );
		assert( testOne != NULL );
		assert( testOne == backup );
		mem_Verify( );
	} mem_CleanUp( );
	llog( LOG_INFO, "Test done." );

	// test parent deallocation also deallocating children
	{
		// make sure a child is released when it's parent is released
		llog( LOG_INFO, "Test that child is released when it's parent is released..." );
		assert( mem_Init( 32 * 1024 ) == 0 ); {
			uint8_t* parent = (uint8_t*)mem_Allocate( 100 );
			uint8_t* child = (uint8_t*)mem_Allocate( 100 );
			bool isParentAllocated = mem_IsAllocatedMemory( parent );
			bool isChildAllocated = mem_IsAllocatedMemory( child );
			assert( isParentAllocated );
			assert( isChildAllocated );
			mem_Verify( );
			mem_Attach( parent, child );
			mem_Release( parent );
			mem_Verify( );
			isParentAllocated = mem_IsAllocatedMemory( parent );
			isChildAllocated = mem_IsAllocatedMemory( child );
			assert( !isParentAllocated );
			assert( !isChildAllocated );
		} mem_CleanUp( );
		llog( LOG_INFO, "Test done." );

		// make sure a memory block that is parented then un-parented is not released when the parent is released
		llog( LOG_INFO, "Test that a memory block that is parented then un-parented is not released when the parent is released..." );
		assert( mem_Init( 32 * 1024 ) == 0 ); {
			uint8_t* parent = (uint8_t*)mem_Allocate( 100 );
			uint8_t* child = (uint8_t*)mem_Allocate( 100 );
			bool isParentAllocated = mem_IsAllocatedMemory( parent );
			bool isChildAllocated = mem_IsAllocatedMemory( child );
			assert( isParentAllocated );
			assert( isChildAllocated );
			mem_Attach( parent, child );
			mem_Verify( );
			mem_DetachFromParent( child );
			mem_Verify( );
			mem_Release( parent );
			mem_Verify( );
			isParentAllocated = mem_IsAllocatedMemory( parent );
			isChildAllocated = mem_IsAllocatedMemory( child );
			assert( !isParentAllocated );
			assert( isChildAllocated );
		} mem_CleanUp( );
		llog( LOG_INFO, "Test done." );

		// make sure a child that is parented to a reallocated parent that is moved is released correctly
		llog( LOG_INFO, "Test that a child that is parented to a reallocated parent that is moved is released correctly..." );
		assert( mem_Init( 32 * 1024 ) == 0 ); {
			uint8_t* parent = (uint8_t*)mem_Allocate( 100 );
			uint8_t* child = (uint8_t*)mem_Allocate( 100 );
			bool isParentAllocated = mem_IsAllocatedMemory( parent );
			bool isChildAllocated = mem_IsAllocatedMemory( child );
			assert( isParentAllocated );
			assert( isChildAllocated );
			mem_Attach( parent, child );
			mem_Verify( );
			parent = (uint8_t*)mem_Resize( parent, 1024 );
			mem_Release( parent );
			mem_Verify( );
			isParentAllocated = mem_IsAllocatedMemory( parent );
			isChildAllocated = mem_IsAllocatedMemory( child );
			assert( !isParentAllocated );
			assert( !isChildAllocated );
		} mem_CleanUp( );
		llog( LOG_INFO, "Test done." );

		// make sure you can't create loops
		llog( LOG_INFO, "Test that you can't recreate loops..." );
		assert( mem_Init( 32 * 1024 ) == 0 ); {
			uint8_t* parent = (uint8_t*)mem_Allocate( 100 );
			uint8_t* child = (uint8_t*)mem_Allocate( 100 );
			uint8_t* grandChild = (uint8_t*)mem_Allocate( 100 );

			bool isParentAllocated = mem_IsAllocatedMemory( parent );
			bool isChildAllocated = mem_IsAllocatedMemory( child );
			bool isGrandChildAllocated = mem_IsAllocatedMemory( child );
			assert( isParentAllocated );
			assert( isChildAllocated );
			assert( isGrandChildAllocated );

			mem_Attach( parent, child );
			mem_Verify( );
			mem_Attach( child, grandChild );
			mem_Verify( );
			bool shouldFail = mem_Attach( grandChild, parent ); // this should fail
			assert( !shouldFail );
			mem_Verify( );
			
			MemoryBlockHeader* grandChildHeader = findMemoryBlock( grandChild, true );
			MemoryBlockHeader* parentHeader = findMemoryBlock( parent, true );
			assert( grandChildHeader != NULL );
			assert( grandChildHeader->firstChild == NULL );
			assert( parentHeader->parent == NULL );
		} mem_CleanUp( );
		llog( LOG_INFO, "Test done." );

		// test deleting any of them and making sure the appropriate ones are also deleted
		// test removing any parent/child relationship and making sure the appropriate ones are deleted
		llog( LOG_INFO, "Test various configurations of detaching and resizing..." );
		const uint32_t MAX_FLAGS = 0b00000000000000000001111111111111;
		for( uint32_t detachFlags = 0; detachFlags <= MAX_FLAGS; ++detachFlags ) {
			for( uint32_t resizeFlags = 0; resizeFlags <= MAX_FLAGS; ++resizeFlags ) {
				llog( LOG_INFO, "  Testing with detach = 0x%x and resize = 0x%x", detachFlags, resizeFlags );
				hierachicalTest( detachFlags, resizeFlags );
			}
		}
		llog( LOG_INFO, "Test done." );
	}

	// restore old memory block
	memoryBlock.memory = oldMemoryBlock;
}