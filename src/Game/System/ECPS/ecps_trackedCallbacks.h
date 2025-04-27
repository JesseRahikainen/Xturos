#ifndef ECPS_TRACKED_CALLBACKS_H
#define ECPS_TRACKED_CALLBACKS_H


#include "ecps_dataTypes.h"
#include "tween.h"
#include "Utils/helpers.h"

// could look into using __attribute__((constructor)) along with a static array of tracked function structures. But VS doesn't support it, so we'd need
//  to find some equivilant, and then we're still getting into all the compiler specific stuff
// it looks like we could do something with the .CRT$XCU section though
// https://stackoverflow.com/questions/1113409/attribute-constructor-equivalent-in-vc
// we could always split it up, have the same API but different ways of doing it for each platform, but then we may get slightly different behavior and
//  it would be nice to debug it in VS if there is an issue
// also doesn't help that there isn't a good way to template out code since we need at least three different versions of this


// this will allow us to declare functions and add them to a tracking section in the code so that we can reference them by id
//  the main restriction is that they'll all need to have the same parameters
// TODO: get this working on Android and iOS

// any function that will be used with a callback that will need to be tracked when we're saving out entities will need to use
//  this so we can restore them after a load
// to add tracking to an existing function:
//   static void testCallback( ECPS* btnEcps, Entity* btn )
//   {
//		llog( LOG_DEBUG, "Do stuff" );
//   }
//   ADD_TRACKED_CALLBACK( "Testing", testCallback );
//
// if for some reason different functions are assigned the same addresses turn off /OPT:IFC, it can cause that

typedef void ( *TrackedCallback )( ECPS*, Entity* );

// don't call the register* functions directly, instead use the ADD_TRACKED_* macros to register as being tracked so it can be saved out when serializing entities
void registerTrackedECPSCallback( const char* id, const TrackedCallback func );
#define ADD_TRACKED_ECPS_CALLBACK( id, fn ) INITIALIZER( register_TrackedECPSCallback_##fn ) { registerTrackedECPSCallback( id, fn ); }

void registerTrackedTweenFunc( const char* id, const EaseFunc func );
#define ADD_TRACKED_TWEEN_FUNC( id, fn ) INITIALIZER( register_TrackedTweenFunc_##fn ) { registerTrackedTweenFunc( id, fn ); }

// checks if there are any null or repeats
void ecps_VerifyCallbackIDs( void );

// get the callback given the id
const TrackedCallback ecps_GetTrackedECPSCallbackFromID( const char* id );
const EaseFunc ecps_GetTrackedTweenFuncFromID( const char* id );

// get the id given the callback
const char* ecps_GetTrackedIDFromECPSCallback( const TrackedCallback callback );
const char* ecps_GetTrackedIDFromTweenFunc( const EaseFunc callback );

// just used for testing, will call every callback with null parameters
void ecps_TestRunAllTrackedCallbacks( void );

#endif