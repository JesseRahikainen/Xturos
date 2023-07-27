#include "ecps_trackedCallbacks.h"

#include <stdbool.h>
#include <SDL_assert.h>

#include "System/platformLog.h"

#include "Utils/hashMap.h"

#if defined(_MSC_VER)
#define section_foreach_tracked_entry(elem)																										\
const TrackedFunctionRef* elem = *(const TrackedFunctionRef* const*)( (uintptr_t)&startTrackedCallbacks + sizeof( startTrackedCallbacks ) );	\
for( uintptr_t current = (uintptr_t)&startTrackedCallbacks + sizeof( startTrackedCallbacks );													\
	 current < (uintptr_t)&endTrackedCallbacks;																									\
	 current += sizeof( const TrackedFunctionRef* ), elem = *(const TrackedFunctionRef* const*)current )
#elif defined(__EMSCRIPTEN__)
extern const TrackedFunctionRef* __start_ecscb;
extern const TrackedFunctionRef* __stop_ecscb;

#define section_foreach_tracked_entry(elem)																						\
const TrackedFunctionRef* elem = *(const TrackedFunctionRef* const*)( (uintptr_t)&__start_ecscb + sizeof( __start_ecscb ) );	\
for( uintptr_t current = (uintptr_t)&__start_ecscb;																				\
	 current < (uintptr_t)&__stop_ecscb;																						\
	 current += sizeof( const TrackedFunctionRef* ), elem = *(const TrackedFunctionRef* const*)current )
#else
#error Tracked callbacks not implmented for this platform.
#endif

void ecps_VerifyCallbackIDs( void )
{
	// would prefer to do this at compile time, but meh
	// would prefer a set, but don't have one so this will do
	HashMap idHashMap;
	hashMap_Init( &idHashMap, 10, NULL );

	section_foreach_tracked_entry( tracked )
	{
		if( tracked ) {
			if( hashMap_Exists( &idHashMap, tracked->id ) ) {
				SDL_assert( false && "Repeated tracked callback id" );
			}
			if( tracked->id == NULL ) {
				SDL_assert( false && "NULL callback id" );
			}
			hashMap_Set( &idHashMap, tracked->id, 1 );
		}
	}

	hashMap_Clear( &idHashMap );
}

void ecps_TestRunAllTrackedCallbacks( void )
{
	llog( LOG_VERBOSE, "Attempting to test all tracked callbacks." );

	section_foreach_tracked_entry( tracked )
	{
		llog( LOG_VERBOSE, "Trying..." );
		if( tracked ) {
			llog( LOG_DEBUG, "id: %s", tracked->id );
			tracked->callback( NULL, NULL );
		}
	}

	llog( LOG_VERBOSE, "All done." );
}

const TrackedCallback ecps_GetTrackedCallbackFromID( const char* id )
{
	section_foreach_tracked_entry( tracked )
	{
		if( tracked ) {
			if( SDL_strcmp( id, tracked->id ) == 0 ) {
				return tracked->callback;
			}
		}
	}

	return NULL;
}

const char* ecps_GetTrackedIDFromCallback( const TrackedCallback callback )
{
	section_foreach_tracked_entry( tracked )
	{
		if( tracked ) {
			if( callback == tracked->callback ) {
				return tracked->id;
			}
		}
	}

	return NULL;
}