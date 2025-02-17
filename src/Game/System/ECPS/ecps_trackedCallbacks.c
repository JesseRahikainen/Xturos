#include "ecps_trackedCallbacks.h"

#include <stdbool.h>
#include <SDL3/SDL_assert.h>

#include "System/platformLog.h"

#include "Utils/hashMap.h"

#if defined(_MSC_VER)
#define section_foreach_tracked_ecps_entry(elem)																												\
const TrackedECPSFunctionRef* elem = *(const TrackedECPSFunctionRef* const*)( (uintptr_t)&startTrackedECPSCallbacks + sizeof( startTrackedECPSCallbacks ) );	\
for( uintptr_t current = (uintptr_t)&startTrackedECPSCallbacks + sizeof( startTrackedECPSCallbacks );															\
	 current < (uintptr_t)&endTrackedECPSCallbacks;																												\
	 current += sizeof( const TrackedECPSFunctionRef* ), elem = *(const TrackedECPSFunctionRef* const*)current )

#define section_foreach_tracked_tween_entry(elem)																											\
const TrackedTweenFunctionRef* elem = *(const TrackedTweenFunctionRef* const*)( (uintptr_t)&startTrackedTweenFuncs + sizeof( startTrackedTweenFuncs ) );	\
for( uintptr_t current = (uintptr_t)&startTrackedTweenFuncs + sizeof( startTrackedTweenFuncs );																\
	 current < (uintptr_t)&endTrackedTweenFuncs;																											\
	 current += sizeof( const TrackedTweenFunctionRef* ), elem = *(const TrackedTweenFunctionRef* const*)current )

#elif defined(__EMSCRIPTEN__)
extern const TrackedECPSFunctionRef* __start_ecscb;
extern const TrackedECPSFunctionRef* __stop_ecscb;

#define section_foreach_tracked_entry(elem)																						\
const TrackedECPSFunctionRef* elem = *(const TrackedECPSFunctionRef* const*)( (uintptr_t)&__start_ecscb + sizeof( __start_ecscb ) );	\
for( uintptr_t current = (uintptr_t)&__start_ecscb;																				\
	 current < (uintptr_t)&__stop_ecscb;																						\
	 current += sizeof( const TrackedECPSFunctionRef* ), elem = *(const TrackedECPSFunctionRef* const*)current )


extern const TrackedTweenFunctionRef* __start_tweenb;
extern const TrackedTweenFunctionRef* __stop_tweenb;

#define section_foreach_tracked_tween_entry(elem)																						\
const TrackedTweenFunctionRef* elem = *(const TrackedTweenFunctionRef* const*)( (uintptr_t)&__start_tweenb + sizeof( __start_tweenb ) );	\
for( uintptr_t current = (uintptr_t)&__start_tweenb;																				\
	 current < (uintptr_t)&__stop_tweenb;																						\
	 current += sizeof( const TrackedTweenFunctionRef* ), elem = *(const TrackedTweenFunctionRef* const*)current )
#else
#error Tracked callbacks not implmented for this platform.
#endif

void ecps_VerifyCallbackIDs( void )
{
	// would prefer to do this at compile time, but meh
	// would prefer a set, but don't have one so this will do
	HashMap idHashMap;
	hashMap_Init( &idHashMap, 10, NULL );

	section_foreach_tracked_ecps_entry( trackedECPS )
	{
		if( trackedECPS ) {
			if( hashMap_Exists( &idHashMap, trackedECPS->id ) ) {
				SDL_assert( false && "Repeated tracked ECPS callback id" );
			}
			if( trackedECPS->id == NULL ) {
				SDL_assert( false && "NULL ECPS callback id" );
			}
			hashMap_Set( &idHashMap, trackedECPS->id, 1 );
		}
	}

	hashMap_Clear( &idHashMap );
	hashMap_Init( &idHashMap, 10, NULL );

	section_foreach_tracked_tween_entry( trackedTween )
	{
		if( trackedTween ) {
			if( hashMap_Exists( &idHashMap, trackedTween->id ) ) {
				SDL_assert( false && "Repeated tracked tween callback id" );
			}
			if( trackedTween->id == NULL ) {
				SDL_assert( false && "NULL tween callback id" );
			}
			hashMap_Set( &idHashMap, trackedTween->id, 1 );
		}
	}
}

void ecps_TestRunAllTrackedCallbacks( void )
{
	llog( LOG_VERBOSE, "Attempting to test all tracked ECPS callbacks." );

	section_foreach_tracked_ecps_entry( tracked )
	{
		llog( LOG_VERBOSE, "Trying..." );
		if( tracked ) {
			llog( LOG_DEBUG, "id: %s", tracked->id );
			tracked->callback( NULL, NULL );
		}
	}

	llog( LOG_VERBOSE, "Attempting to test all tracked tween funcs." );
	section_foreach_tracked_tween_entry( trackedTween )
	{
		llog( LOG_VERBOSE, "Trying..." );
		if( trackedTween ) {
			llog( LOG_DEBUG, "id: %s   %f", trackedTween->id, trackedTween->func( 0.0f ) );
		}
	}

	llog( LOG_VERBOSE, "All done." );
}

const TrackedCallback ecps_GetTrackedECPSCallbackFromID( const char* id )
{
	section_foreach_tracked_ecps_entry( tracked )
	{
		if( tracked ) {
			if( SDL_strcmp( id, tracked->id ) == 0 ) {
				return tracked->callback;
			}
		}
	}

	return NULL;
}

const EaseFunc ecps_GetTrackedTweenFuncFromID( const char* id )
{
	section_foreach_tracked_tween_entry( tracked )
	{
		if( tracked ) {
			if( SDL_strcmp( id, tracked->id ) == 0 ) {
				return tracked->func;
			}
		}
	}

	return NULL;
}

const char* ecps_GetTrackedIDFromECPSCallback( const TrackedCallback callback )
{
	section_foreach_tracked_ecps_entry( tracked )
	{
		if( tracked ) {
			if( callback == tracked->callback ) {
				return tracked->id;
			}
		}
	}

	return NULL;
}

const char* ecps_GetTrackedIDFromTweenFunc( const EaseFunc callback )
{
	section_foreach_tracked_tween_entry( tracked )
	{
		if( tracked ) {
			if( callback == tracked->func ) {
				return tracked->id;
			}
		}
	}

	return NULL;
}