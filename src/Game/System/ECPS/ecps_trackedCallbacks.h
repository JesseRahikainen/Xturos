#ifndef ECPS_TRACKED_CALLBACKS_H
#define ECPS_TRACKED_CALLBACKS_H

#include "ecps_dataTypes.h"

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
// to declare the callback with the tracking:
//   DECLARE_TRACKED_CALLBACK( "Testing", testCallback, btnEcps, btn )
//   {
//		llog( LOG_DEBUG, "Do stuff" );
//   }
//
// these are equivilant
// if for some reason different functions are assigned the same addresses turn off /OPT:IFC, it can cause that

typedef void ( *TrackedCallback )( ECPS*, Entity* );
typedef struct {
	const char* id;
	TrackedCallback callback;
} TrackedFunctionRef;

// MSVC version
// references:
// https://learn.microsoft.com/en-us/cpp/build/reference/section-specify-section-attributes?view=msvc-170
// https://learn.microsoft.com/en-us/cpp/preprocessor/section?view=msvc-170
// https://devblogs.microsoft.com/oldnewthing/20181107-00/?p=100155
// https://devblogs.microsoft.com/oldnewthing/20181108-00/?p=100165
// https://devblogs.microsoft.com/oldnewthing/20181109-00/?p=100175

// for GCC version look into these, will be something similar
// https://mgalgs.io/2013/05/10/hacking-your-ELF-for-fun-and-profit.html
// https://stackoverflow.com/questions/3633896/append-items-to-an-array-with-a-macro-in-c
// https://stackoverflow.com/questions/4152018/initialize-global-array-of-function-pointers-at-either-compile-time-or-run-time
// https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html

#if defined( _MSC_VER )
#pragma section("ecscb$a", read) // beginning
#pragma section("ecscb$m", read) // storage
#pragma section("ecscb$z", read) // end
#define FORCE_INTO_BUILD(n) __pragma( comment( linker, "/include:"#n))
#define ADD_TO_TRACKED_CALLBACK_SECTION __declspec( allocate( "ecscb$m" ) ) 
__declspec( allocate( "ecscb$a" ) ) static const TrackedFunctionRef* startTrackedCallbacks = NULL;
__declspec( allocate( "ecscb$z" ) ) static const TrackedFunctionRef* endTrackedCallbacks = NULL;
#define USES_STATIC
#elif defined( __EMSCRIPTEN__ )
#include <emscripten.h>
// we have to tell the link to include these sections as well, retain attri
#define FORCE_INTO_BUILD(n)
#define ADD_TO_TRACKED_CALLBACK_SECTION __attribute__( ( used, retain, section( "ecscb" ) ) )
// using the "used" attribute and that only works on static
#define USES_STATIC static
#else
#error Tracked callbacks not implmented for this platform.
#endif

#define ADD_TRACKED_CALLBACK(id, fn) \
	USES_STATIC const TrackedFunctionRef trackedFunction##fn = { id, fn }; \
	ADD_TO_TRACKED_CALLBACK_SECTION const TrackedFunctionRef* trackedFunction##fn##ptr = &trackedFunction##fn; \
	FORCE_INTO_BUILD("trackedFunction"#fn) \
	FORCE_INTO_BUILD("trackedFunction"#fn"ptr")

#define DECLARE_TRACKED_CALLBACK(id, fn, ecpsName, entityName) \
	void fn(ECPS*, Entity*); \
	ADD_TRACKED_CALLBACK(id, fn) \
	void fn(ECPS* ecpsName, Entity* entityName)

// checks if there are any null or repeats
void ecps_VerifyCallbackIDs( void );

// get the callback given the id
const TrackedCallback ecps_GetTrackedCallbackFromID( const char* id );

// get the id given the callback
const char* ecps_GetTrackedIDFromCallback( const TrackedCallback callback );

// just used for testing, will call every callback with null parameters
void ecps_TestRunAllTrackedCallbacks( void );

#endif