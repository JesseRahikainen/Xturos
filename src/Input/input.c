#include "input.h"
#include <float.h>
#include <assert.h>
#include "../System/platformLog.h"

/***** Key Binding *****/

typedef struct {
	SDL_Keycode code;
	KeyResponse response;
} KeyBindings;

typedef struct {
	Uint8 button;
	KeyResponse response;
} MouseButtonBindings;

#define MAX_BINDINGS 32

static KeyBindings keyDownBindings[MAX_BINDINGS];
static KeyBindings keyUpBindings[MAX_BINDINGS];

static MouseButtonBindings mouseButtonDownBindings[MAX_BINDINGS];
static MouseButtonBindings mouseButtonUpBindings[MAX_BINDINGS];

/*
Clears all the current key bindings.
*/
void input_ClearAllKeyBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		keyDownBindings[i].response = NULL;
		keyUpBindings[i].response = NULL;
	}
}

/*
Clears all key bindings assocated with the passed in key code.
*/
void input_ClearKeyBinds( SDL_Keycode code )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( keyDownBindings[i].code == code ) {
			keyDownBindings[i].response = NULL;
		}
		if( keyUpBindings[i].code == code ) {
			keyUpBindings[i].response = NULL;
		}
	}
}

/*
Clears all key bindings associated with the passed in response function.
*/
void input_ClearKeyResponse( KeyResponse response )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( keyDownBindings[i].response == response ) {
			keyDownBindings[i].response = NULL;
		}
		if( keyUpBindings[i].response == response ) {
			keyUpBindings[i].response = NULL;
		}
	}
}

/*
Finds the first unused binding in the list.
 Return < 0 if we don't find a spot.
*/
int bindToList( SDL_Keycode code, KeyResponse response, KeyBindings* bindingsList )
{
	if( response == NULL ) {
		llog( LOG_DEBUG, "Attempting to bind a key with a NULL response." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response != NULL ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Key binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].code = code;
	bindingsList[idx].response = response;

	return 0;
}

/*
Binds a function response when a key is pressed down.
*/
int input_BindOnKeyPress( SDL_Keycode code, KeyResponse response )
{
	if( bindToList( code, response, keyDownBindings ) < 0 ) {
		return -1;
	}

	return 0;
}

/*
Binds a function response when a key is released.
*/
int input_BindOnKeyRelease( SDL_Keycode code, KeyResponse response )
{
	if( bindToList( code, response, keyUpBindings ) < 0 ) {
		return -1;
	}

	return 0;
}

/*
Gets the code associated with response function. Puts them into the keyCodes array (which should be no larger than maxKeyCodes).
 The rest of the array is filled with SDLK_UNKNOWN.
*/
void getKeyBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes, KeyBindings* bindingList )
{
	int keyCodeIdx = 0;

	for( int i = 0; ( i < MAX_BINDINGS ) && ( keyCodeIdx < maxKeyCodes ); ++i ) {
		if( bindingList[i].response == response ) {
			keyCodes[keyCodeIdx] = bindingList[i].code;
			++keyCodeIdx;
		}
	}

	for( ; keyCodeIdx < maxKeyCodes; ++keyCodeIdx ) {
		keyCodes[keyCodeIdx] = SDLK_UNKNOWN;
	}
}

void input_GetKeyPressBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes )
{
	getKeyBindings( response, keyCodes, maxKeyCodes, keyDownBindings );
}

void input_GetKeyReleaseBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes )
{
	getKeyBindings( response, keyCodes, maxKeyCodes, keyUpBindings );
}

/*
Handles a key event.
*/
void handleKeyEvent( SDL_Keycode code, KeyBindings* bindingsList )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( ( bindingsList[i].code == code ) && ( bindingsList[i].response != NULL ) ) {
			bindingsList[i].response( );
		}
	}
}

/***** Mouse Input *****/
static Vector2 mousePosition;
static Vector2 rawMousePosition;
static int mousePosValid;

static Vector2 mouseInputArea;
static Vector2 mouseInputOffset;
static float mouseInputScale;

/*
Sets up the coordinates for relative mouse position handling.
*/
void input_InitMouseInputArea( int inputWidth, int inputHeight )
{
	mousePosValid = 0;
	mousePosition = VEC2_ZERO;
	rawMousePosition = VEC2_ZERO;

	mouseInputArea.x = (float)inputWidth;
	mouseInputArea.y = (float)inputHeight;
	mouseInputOffset = VEC2_ZERO;
	mouseInputScale = 1.0f;
}

/*
Sets the size of the window, used for getting relative mouse position.
*/
void input_UpdateMouseWindow( int windowWidth, int windowHeight )
{
	mousePosValid = 0;
	mousePosition = VEC2_ZERO;

	Vector2 windowSize;

	// calculates the offset and scaling necessary to convert the window mouse position to the desired input area
	//  assumes that the input area is centered in the window
	windowSize.x = (float)windowWidth;
	windowSize.y = (float)windowHeight;

	float windowRatio = windowSize.x / windowSize.y;
	float inputRatio = mouseInputArea.x / mouseInputArea.y;

	if( windowRatio < inputRatio ) {
		mouseInputOffset.x = 0.0f;
		mouseInputOffset.y = -( windowSize.y - ( windowSize.x / inputRatio ) ) / 2.0f;

		mouseInputScale = mouseInputArea.x / windowSize.x;
	} else {
		mouseInputOffset.x = -( windowSize.x - ( windowSize.y * inputRatio ) ) / 2.0f;
		mouseInputOffset.y = 0.0f;

		mouseInputScale = mouseInputArea.y / windowSize.y;
	}
}

/*
Gets the relative position of the mouse.
 Returns 1 if the position is inside the input area.
*/
int input_GetMousePosition( Vector2* out )
{
	(*out) = mousePosition;
	return mousePosValid;
}

/*
Gets the position of the mouse in the window.
*/
void input_GetRawMousePosition( Vector2* out )
{
	(*out) = rawMousePosition;
}

/*
Clears all current mouse button bindings.
*/
void input_ClearAllMouseButtonBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		mouseButtonDownBindings[i].response = NULL;
		mouseButtonUpBindings[i].response = NULL;
	}
}

/*
Clears all key bindings assocated with the passed in key code.
*/
void input_ClearMouseButtonBinds( Uint8 button )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( mouseButtonDownBindings[i].button == button ) {
			mouseButtonDownBindings[i].response = NULL;
		}
		if( mouseButtonUpBindings[i].button == button ) {
			mouseButtonUpBindings[i].response = NULL;
		}
	}
}

/*
Clears all mouse button bindsings associated with the passed in response.
*/
void input_ClearMouseButtonReponse( KeyResponse response )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( mouseButtonDownBindings[i].response == response ) {
			mouseButtonDownBindings[i].response = NULL;
		}
		if( mouseButtonUpBindings[i].response == response ) {
			mouseButtonUpBindings[i].response = NULL;
		}
	}
}

/*
Finds the first unused binding in the list.
 Return < 0 if we don't find a spot.
*/
int bindToMouseList( Uint8 button, KeyResponse response, MouseButtonBindings* bindingsList )
{
	if( response == NULL ) {
		llog( LOG_DEBUG, "Attempting to bind a mouse button with a NULL response." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response != NULL ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Mouse button binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].button = button;
	bindingsList[idx].response = response;

	return 0;
}

/*
Binds a function response when a key is pressed down.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnMouseButtonPress( Uint8 button, KeyResponse response )
{
	return bindToMouseList( button, response, mouseButtonDownBindings );
}

/*
Binds a function response when a key is released.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnMouseButtonRelease( Uint8 button, KeyResponse response )
{
	return bindToMouseList( button, response, mouseButtonUpBindings );
}

static void processMouseMovementEvent( SDL_Event* e )
{
	rawMousePosition.x = (float)( e->motion.x );
	rawMousePosition.y = (float)( e->motion.y );

	// adjust the position from the window to the mouse input area
	vec2_Add( &rawMousePosition, &mouseInputOffset, &mousePosition );
	vec2_Scale( &mousePosition, mouseInputScale, &mousePosition );

	// if it's in the input area then the position is valid, otherwise it isn't
	mousePosValid = ( ( mousePosition.x >= 0.0f ) &&
					  ( mousePosition.y >= 0.0f ) &&
					  ( mousePosition.x <= mouseInputArea.x ) &&
					  ( mousePosition.y <= mouseInputArea.y ) );
}

/*
Handles a mouse button event.
*/
void handleMouseButtonEvent( Uint8 button, MouseButtonBindings* bindingsList )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( ( bindingsList[i].button == button ) && ( bindingsList[i].response != NULL ) ) {
			bindingsList[i].response( );
		}
	}
}

/***** Gesture Recognition *****/
// This pulls from the mouse position (and by proxy the primary touch as well)
typedef struct {
	Vector2 dir;
	KeyResponse response;
} SwipeBinding;

static SwipeBinding swipeBindings[MAX_BINDINGS];
#define MIN_SWIPE_DIST 0.1f
#define MAX_SWIPE_ERROR 0.1f;

/*
Clears all binded swipes.
*/
void input_ClearAllSwipeBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		swipeBindings[i].response = NULL;
	}
}

/*
Binds a function response when a swipe is made in a specific direction.
 Returns < 0 if there was a problem binding the swipe.
*/
int input_BindOnSwipe( Vector2 dir, KeyResponse response )
{
	vec2_Normalize( &dir );
	assert( vec2_Mag( &dir ) > 0.01f );

	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( swipeBindings[i].response == NULL ) {
			swipeBindings[i].dir = dir;
			swipeBindings[i].response = response;

			return 0;
		}
	}

	return -1;
}

static SDL_FingerID watchedFinger = -1;
static Vector2 watchedFingerPosStart;

static void processSwipePress( SDL_Event* e )
{
	if( watchedFinger < 0 ) {
		watchedFingerPosStart.x = e->tfinger.x;
		watchedFingerPosStart.y = e->tfinger.y;
		watchedFinger = e->tfinger.fingerId;
	}
}

static void processSwipeRelease( SDL_Event* e )
{
	if( e->tfinger.fingerId == watchedFinger ) {
		Vector2 endPos;
		endPos.x = e->tfinger.x;
		endPos.y = e->tfinger.y;

		Vector2 diff;

		vec2_Subtract( &endPos, &watchedFingerPosStart, &diff );

		if( vec2_MagSqrd( &diff ) >= ( MIN_SWIPE_DIST * MIN_SWIPE_DIST ) ) {
			vec2_Normalize( &diff );
			
			float bestError = FLT_MAX;
			KeyResponse response = NULL;

			for( int i = 0; i < MAX_BINDINGS; ++i ) {
				if( swipeBindings[i].response == NULL ) continue;

				float error = vec2_DotProduct( &diff, &( swipeBindings[i].dir ) );

				if( error < bestError ) {
					bestError = error;
					response = swipeBindings[i].response;
				}
			}

			if( response != NULL ) {
				response( );
			}
		}

		watchedFinger = -1;
	}
}

/***** General Usage Functions *****/
/*
Process events.
*/
void input_ProcessEvents( SDL_Event* e )
{
	switch( e->type ) {
	case SDL_MOUSEMOTION:
		processMouseMovementEvent( e );
		break;
	case SDL_KEYDOWN:
		if( !e->key.repeat ) {
			handleKeyEvent( e->key.keysym.sym, keyDownBindings );
		}
		break;
	case SDL_KEYUP:
		handleKeyEvent( e->key.keysym.sym, keyUpBindings );
		break;
	case SDL_MOUSEBUTTONDOWN:
		handleMouseButtonEvent( e->button.button, mouseButtonDownBindings );
		break;
	case SDL_MOUSEBUTTONUP:
		handleMouseButtonEvent( e->button.button, mouseButtonUpBindings );
		break;
	case SDL_FINGERDOWN:
		processSwipePress( e );
		break;
	case SDL_FINGERUP:
		processSwipeRelease( e );
		break;
	}
}