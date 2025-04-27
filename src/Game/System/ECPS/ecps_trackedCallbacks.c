#include "ecps_trackedCallbacks.h"

#include <stdbool.h>
#include <SDL3/SDL_assert.h>
#include <assert.h>

#include "System/platformLog.h"

#include "Utils/hashMap.h"

typedef struct {
	const char* id;
	TrackedCallback callback;
} TrackedECPSFunctionRef;
#define MAX_TRACKED_ECPS_FUNCTIONS 64
static TrackedECPSFunctionRef trackedECPSFunctions[MAX_TRACKED_ECPS_FUNCTIONS];
static int trackedECPSFunctionsCount = 0;

void registerTrackedECPSCallback( const char* id, const TrackedCallback func )
{
	assert( trackedECPSFunctionsCount < MAX_TRACKED_ECPS_FUNCTIONS );
	if( trackedECPSFunctionsCount < MAX_TRACKED_ECPS_FUNCTIONS ) {
		trackedECPSFunctions[trackedECPSFunctionsCount].id = id;
		trackedECPSFunctions[trackedECPSFunctionsCount].callback = func;
		++trackedECPSFunctionsCount;
	}
}

#define section_foreach_tracked_ecps_entry(elem)											\
	const TrackedECPSFunctionRef* elem = &trackedECPSFunctions[0];							\
	for( int i = 0; i < trackedECPSFunctionsCount; ++i, elem = &trackedECPSFunctions[i] )



typedef struct {
	const char* id;
	EaseFunc func;
} TrackedTweenFunctionRef;

#define MAX_TRACKED_TWEEN_FUNCTIONS 40
static TrackedTweenFunctionRef trackedTweenFunctions[MAX_TRACKED_TWEEN_FUNCTIONS];
static int trackedTweenFunctionsCount = 0;
void registerTrackedTweenFunc( const char* id, const EaseFunc func )
{
	assert( trackedECPSFunctionsCount < MAX_TRACKED_TWEEN_FUNCTIONS );
	if( trackedTweenFunctionsCount < MAX_TRACKED_TWEEN_FUNCTIONS ) {
		trackedTweenFunctions[trackedTweenFunctionsCount].id = id;
		trackedTweenFunctions[trackedTweenFunctionsCount].func = func;
		++trackedTweenFunctionsCount;
	}
}

#define section_foreach_tracked_tween_entry(elem)											\
	const TrackedTweenFunctionRef* elem = &trackedTweenFunctions[0];						\
	for( int i = 0; i < trackedTweenFunctionsCount; ++i, elem = &trackedTweenFunctions[i] )

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