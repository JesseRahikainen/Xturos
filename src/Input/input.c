#include "input.h"

#include <SDL_log.h>

/***** Key Binding *****/

typedef struct {
	SDL_Keycode code;
	KeyResponse response;
} KeyBindings;

#define MAX_BINDINGS 32

static KeyBindings keyDownBindings[MAX_BINDINGS];
static KeyBindings keyUpBindings[MAX_BINDINGS];

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
Finds the first unused binding in the list.
 Return < 0 if we don't find a spot.
*/
int bindToList( SDL_Keycode code, KeyResponse response, KeyBindings* bindingsList )
{
	if( response == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, "Attempting to bind a key with a NULL response." );
		return -1;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response != NULL ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, "Key binding list full, increase size of bind list." );
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

/***** Mouse Position Input *****/
static Vector2 mousePosition;
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
int input_GetMousePostion( Vector2* out )
{
	(*out) = mousePosition;
	return mousePosValid;
}

static void processMouseMovementEvent( SDL_Event* e )
{
	mousePosition.x = (float)( e->motion.x );
	mousePosition.y = (float)( e->motion.y );

	// adjust the position from the window to the mouse input area
	vec2_Add( &mousePosition, &mouseInputOffset, &mousePosition );
	vec2_Scale( &mousePosition, mouseInputScale, &mousePosition );

	// if it's in the input area then the position is valid, otherwise it isn't
	mousePosValid = ( ( mousePosition.x >= 0.0f ) &&
					  ( mousePosition.y >= 0.0f ) &&
					  ( mousePosition.x <= mouseInputArea.x ) &&
					  ( mousePosition.y <= mouseInputArea.y ) );
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
	}
}