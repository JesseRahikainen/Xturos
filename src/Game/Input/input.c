#include "input.h"
#include <float.h>
#include <SDL3/SDL_assert.h>
#include "System/platformLog.h"
#include "System/shared.h"
#include "System/luaInterface.h"
#include "System/memory.h"

/***** Key Binding *****/

#define SCRIPT_FUNC_NAME_MAX_LEN 64

typedef struct {
	CallbackType type;
	KeyResponse response;
} SourceResponse;

typedef struct {
	CallbackType type;
	char response[SCRIPT_FUNC_NAME_MAX_LEN];
} ScriptResponse;

typedef union {
	CallbackType type;
	SourceResponse sourceResponse;
	ScriptResponse scriptResponse;
} Response;

typedef struct {
	SDL_Keycode code;
	Response response;
} KeyBindings;

typedef struct {
	Uint8 button;
	Response response;
} MouseButtonBindings;

#define MAX_BINDINGS 32

static KeyBindings keyDownBindings[MAX_BINDINGS];
static KeyBindings keyUpBindings[MAX_BINDINGS];

static MouseButtonBindings mouseButtonDownBindings[MAX_BINDINGS];
static MouseButtonBindings mouseButtonUpBindings[MAX_BINDINGS];

static void runResponse( Response* response )
{
	SDL_assert( response != NULL );

	if( response->type == CBT_SOURCE ) {
		if( response->sourceResponse.response != NULL ) response->sourceResponse.response( );
	} else if( response->type == CBT_LUA ) {
		if( response->scriptResponse.response[0] != 0 ) xLua_CallLuaFunction( response->scriptResponse.response, "" );
	} else {
		llog( LOG_ERROR, "Unknown callback type." );
	}
}

static void clearResponse( Response* response )
{
	SDL_assert( response != NULL );
	response->type = CBT_NONE;
}

/*
Clears all the current key bindings.
*/
void input_ClearAllKeyBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		clearResponse( &keyDownBindings[i].response );
		clearResponse( &keyUpBindings[i].response );
	}
}

/*
Clears all key bindings assocated with the passed in key code.
*/
void input_ClearKeyBinds( SDL_Keycode code )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( keyDownBindings[i].code == code ) {
			clearResponse( &keyDownBindings[i].response );
		}
		if( keyUpBindings[i].code == code ) {
			clearResponse( &keyUpBindings[i].response );
		}
	}
}

/*
Clears all key bindings associated with the passed in response function.
*/
void input_ClearKeyResponse( KeyResponse response )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( ( keyDownBindings[i].response.type == CBT_SOURCE ) && ( keyDownBindings[i].response.sourceResponse.response == response ) ) {
			clearResponse( &keyDownBindings[i].response );
		}

		if( ( keyUpBindings[i].response.type == CBT_SOURCE ) && ( keyUpBindings[i].response.sourceResponse.response == response ) ) {
			clearResponse( &keyUpBindings[i].response );
		}
	}
}

/*
Finds the first unused binding in the list.
 Return < 0 if we don't find a spot.
*/
static int bindToList( SDL_Keycode code, KeyResponse response, KeyBindings* bindingsList )
{
	if( response == NULL ) {
		llog( LOG_DEBUG, "Attempting to bind a key with a NULL response." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Key binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].code = code;
	bindingsList[idx].response.type = CBT_SOURCE;
	bindingsList[idx].response.sourceResponse.response = response;

	return 0;
}

static int bindScriptToList( SDL_Keycode code, const char* response, KeyBindings* bindingsList )
{
	size_t responseLen = SDL_strlen( response );
	if( responseLen == 0 ) {
		llog( LOG_DEBUG, "Attempting to bind a key with no script response." );
		return -1;
	}

	if( responseLen >= SCRIPT_FUNC_NAME_MAX_LEN ) {
		llog( LOG_DEBUG, "Attempting to bind a key with a script reponse name that is too long." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Key binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].code = code;
	bindingsList[idx].response.type = CBT_LUA;
	SDL_strlcpy( bindingsList[idx].response.scriptResponse.response, response, SCRIPT_FUNC_NAME_MAX_LEN );

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

int input_BindScriptOnKeyPress( SDL_Keycode code, const char* func )
{
	if( bindScriptToList( code, func, keyDownBindings ) < 0 ) {
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

int input_BindScriptOnKeyRelease( SDL_Keycode code, const char* func )
{
	if( bindScriptToList( code, func, keyUpBindings ) < 0 ) {
		return -1;
	}

	return 0;
}

/*
Binds function response when a key is pressed and released.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnKey( SDL_Keycode code, KeyResponse onPressResponse, KeyResponse onReleaseResponse )
{
	int onPress = 0;
	if( onPressResponse != NULL ) {
		onPress = input_BindOnKeyPress( code, onPressResponse );
	}

	int onRelease = 0;
	if( onReleaseResponse != NULL ) {
		onRelease = input_BindOnKeyRelease( code, onReleaseResponse );
	}

	if( ( onPress < 0 ) || ( onRelease < 0 ) ) {
		return -1;
	}
	return 0;
}

int input_BindScriptOnKey( SDL_Keycode code, const char* onPressFunc, const char* onReleaseFunc )
{
	int onPress = 0;
	if( SDL_strlen( onPressFunc ) > 0 ) {
		onPress = input_BindScriptOnKeyPress( code, onPressFunc );
	}

	int onRelease = 0;
	if( SDL_strlen( onReleaseFunc ) > 0 ) {
		onRelease = input_BindScriptOnKeyRelease( code, onReleaseFunc );
	}

	if( ( onPress < 0 ) || ( onRelease < 0 ) ) {
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
		if( ( bindingList[i].response.type == CBT_SOURCE ) && ( bindingList[i].response.sourceResponse.response == response ) ) {
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
		if( ( bindingsList[i].code == code ) && ( bindingsList[i].response.type != CBT_NONE ) ) {
			runResponse( &bindingsList[i].response );
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
		clearResponse( &mouseButtonDownBindings[i].response );
		clearResponse( &mouseButtonUpBindings[i].response );
	}
}

/*
Clears all key bindings assocated with the passed in key code.
*/
void input_ClearMouseButtonBinds( Uint8 button )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( mouseButtonDownBindings[i].button == button ) {
			clearResponse( &mouseButtonDownBindings[i].response );
		}
		if( mouseButtonUpBindings[i].button == button ) {
			clearResponse( &mouseButtonUpBindings[i].response );
		}
	}
}

/*
Clears all mouse button bindsings associated with the passed in response.
*/
void input_ClearMouseButtonReponse( KeyResponse response )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( ( mouseButtonDownBindings[i].response.type == CBT_SOURCE ) && ( mouseButtonDownBindings[i].response.sourceResponse.response == response ) ) {
			clearResponse( &mouseButtonDownBindings[i].response );
		}
		if( ( mouseButtonUpBindings[i].response.type == CBT_SOURCE ) && ( mouseButtonUpBindings[i].response.sourceResponse.response == response ) ) {
			clearResponse( &mouseButtonUpBindings[i].response );
		}
	}
}

/*
Finds the first unused binding in the list.
 Return < 0 if we don't find a spot.
*/
static int bindToMouseList( Uint8 button, KeyResponse response, MouseButtonBindings* bindingsList )
{
	if( response == NULL ) {
		llog( LOG_DEBUG, "Attempting to bind a mouse button with a NULL response." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Mouse button binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].button = button;
	bindingsList[idx].response.type = CBT_SOURCE;
	bindingsList[idx].response.sourceResponse.response = response;

	return 0;
}

static int bindScriptToMouseList( Uint8 button, const char* response, MouseButtonBindings* bindingsList )
{
	size_t responseLen = SDL_strlen( response );
	if( responseLen == 0 ) {
		llog( LOG_DEBUG, "Attempting to bind a script to mouse button with no reponse." );
		return -1;
	}

	if( responseLen >= SCRIPT_FUNC_NAME_MAX_LEN ) {
		llog( LOG_DEBUG, "Attempting to bind a script to mouse button with a reponse with too long a name." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Mouse button binding list full, increase size of bind list." );
		return -1;
	}

	bindingsList[idx].button = button;
	bindingsList[idx].response.type = CBT_LUA;
	SDL_strlcpy( bindingsList[idx].response.scriptResponse.response, response, SCRIPT_FUNC_NAME_MAX_LEN );

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

int input_BindScriptOnMouseButtonPress( Uint8 button, const char* response )
{
	return bindScriptToMouseList( button, response, mouseButtonDownBindings );
}

/*
Binds a function response when a key is released.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnMouseButtonRelease( Uint8 button, KeyResponse response )
{
	return bindToMouseList( button, response, mouseButtonUpBindings );
}

int input_BindScriptOnMouseButtonRelease( Uint8 button, const char* response )
{
	return bindScriptToMouseList( button, response, mouseButtonUpBindings );
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
		if( ( bindingsList[i].button == button ) && ( bindingsList[i].response.type != CBT_NONE ) ) {
			runResponse( &bindingsList[i].response );
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
	SDL_assert( vec2_Mag( &dir ) > 0.01f );

	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		if( swipeBindings[i].response == NULL ) {
			swipeBindings[i].dir = dir;
			swipeBindings[i].response = response;

			return 0;
		}
	}

	return -1;
}

static SDL_FingerID watchedFinger = 0;
static Vector2 watchedFingerPosStart;

static void processSwipePress( SDL_Event* e )
{
	if( watchedFinger < 0 ) {
		watchedFingerPosStart.x = e->tfinger.x;
		watchedFingerPosStart.y = e->tfinger.y;
		watchedFinger = e->tfinger.fingerID;
	}
}

static void processSwipeRelease( SDL_Event* e )
{
	if( e->tfinger.fingerID == watchedFinger ) {
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

		watchedFinger = 0;
	}
}

/***** General Usage Functions *****/
/*
Process events.
*/
void input_ProcessEvents( SDL_Event* e )
{
	switch( e->type ) {
	case SDL_EVENT_MOUSE_MOTION:
		processMouseMovementEvent( e );
		break;
	case SDL_EVENT_KEY_DOWN:
		if( !e->key.repeat ) {
			handleKeyEvent( e->key.key, keyDownBindings );
		}
		break;
	case SDL_EVENT_KEY_UP:
		handleKeyEvent( e->key.key, keyUpBindings );
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		handleMouseButtonEvent( e->button.button, mouseButtonDownBindings );
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		handleMouseButtonEvent( e->button.button, mouseButtonUpBindings );
		break;
	case SDL_EVENT_FINGER_DOWN:
		processSwipePress( e );
		break;
	case SDL_EVENT_FINGER_UP:
		processSwipeRelease( e );
		break;
	}
}