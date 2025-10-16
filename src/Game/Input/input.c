#include "input.h"
#include <float.h>
#include <SDL3/SDL_assert.h>
#include "System/platformLog.h"
#include "System/shared.h"
#include "System/memory.h"
#include "Math/matrix3.h"
#include "Graphics/graphics.h"
#include "Graphics/camera.h"
#include "System/messageBroadcast.h"

static void handleRedoMouseArea( void* data )
{
	input_SetupMouseArea( );
}

bool input_Init( void )
{
	input_SetupMouseArea( );
	mb_RegisterListener( MSG_RENDER_RESIZED, handleRedoMouseArea );
	mb_RegisterListener( MSG_WINDOW_RESIZED, handleRedoMouseArea );
	return true;
}

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
		SDL_assert( false && "Doesn't work in this version of the engine." );
		//if( response->scriptResponse.response[0] != 0 ) xLua_CallLuaFunction( response->scriptResponse.response, "" );
	} else {
		llog( LOG_ERROR, "Unknown callback type." );
	}
}

static void clearResponse( Response* response )
{
	SDL_assert( response != NULL );
	response->type = CBT_NONE;
}

// Clears all the current key bindings.
void input_ClearAllKeyBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		clearResponse( &keyDownBindings[i].response );
		clearResponse( &keyUpBindings[i].response );
	}
}

// Clears all key bindings assocated with the passed in key code.
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

// Clears all key bindings associated with the passed in response function.
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

// Finds the first unused binding in the list.
//  Return < 0 if we don't find a spot.
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

// Binds a function response when a key is pressed down.
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

// Binds a function response when a key is released.
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

// Binds function response when a key is pressed and released.
//  Returns < 0 if there was a problem binding the key.
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

// Gets the code associated with response function. Puts them into the keyCodes array (which should be no larger than maxKeyCodes).
//  The rest of the array is filled with SDLK_UNKNOWN.
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

// Handles a key event.
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
static bool mousePosValid;

static Vector2 mouseInputArea;
static Vector2 mouseInputOffset;
static float mouseInputScale;

// Sets up the coordinates for relative mouse position handling. Should be called after, and is only valid after, any changes to the render area or window size.
void input_SetupMouseArea( void )
{
	mousePosValid = false;
	mousePosition = VEC2_ZERO;
	rawMousePosition = VEC2_ZERO;

	// this is to calculate the conversion from window space to render space
	int winX0, winY0, winX1, winY1;
	gfx_GetWindowSubArea( &winX0, &winY0, &winX1, &winY1 );
	Vector2 winMin = vec2( (float)winX0, (float)winY0 );
	Vector2 winMax = vec2( (float)winX1, (float)winY1 );

	int renderX0, renderY0, renderX1, renderY1;
	gfx_GetRenderSubArea( &renderX0, &renderY0, &renderX1, &renderY1 );
	Vector2 renderMin = vec2( (float)renderX0, (float)renderY0 );
	Vector2 renderMax = vec2( (float)renderX1, (float)renderY1 );

	vec2_Subtract( &renderMax, &renderMin, &mouseInputArea );

	// the final equation will be: mousePos = ( rawMousePos + mouseInputOffset ) * mouseInputScale
	vec2_Subtract( &renderMin, &winMin, &mouseInputOffset );
	// we're assuming the ratio of the window and render areas are the same
	mouseInputScale = ( renderMax.x - renderMin.x ) / ( winMax.x - winMin.x );

	if( gfx_GetRenderResizeType( ) == RRT_FIT_RENDER_OUTSIDE ) {
		vec2_Scale( &mouseInputOffset, 1.0f / mouseInputScale, &mouseInputOffset );
	}

	llog( LOG_DEBUG, "mouseInputOffset: <%.02f, %.02f>, mouseInputScale: %.02f", mouseInputOffset.x, mouseInputOffset.y, mouseInputScale );
}

// returns if the mouse is inside the window
bool input_MousePositionValid( void )
{
	return mousePosValid;
}

// Gets the relative position of the mouse.
//  Returns if the position is inside the input area.
bool input_GetMousePosition( Vector2* out )
{
	(*out) = mousePosition;
	return mousePosValid;
}

// Gets the position of the mouse in the window.
void input_GetRawMousePosition( Vector2* out )
{
	(*out) = rawMousePosition;
}

// Get the world position relative to a camera
bool input_GetCamMousePos( int camera, Vector2* out )
{
	Vector2 mp;
	input_GetMousePosition( &mp );
	Matrix4 invView;
	cam_GetInverseViewMatrix( 0, &invView );
	mat4_TransformVec2Pos( &invView, &mp, out );

	return mousePosValid;
}

// Clears all current mouse button bindings.
void input_ClearAllMouseButtonBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		clearResponse( &mouseButtonDownBindings[i].response );
		clearResponse( &mouseButtonUpBindings[i].response );
	}
}

// Clears all key bindings assocated with the passed in key code.
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

// Clears all mouse button bindsings associated with the passed in response.
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

// Finds the first unused binding in the list.
//  Return false if we don't find a spot, true if we do.
static bool bindToMouseList( Uint8 button, KeyResponse response, MouseButtonBindings* bindingsList )
{
	if( response == NULL ) {
		llog( LOG_DEBUG, "Attempting to bind a mouse button with a NULL response." );
		return false;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Mouse button binding list full, increase size of bind list." );
		return false;
	}

	bindingsList[idx].button = button;
	bindingsList[idx].response.type = CBT_SOURCE;
	bindingsList[idx].response.sourceResponse.response = response;

	return true;
}

static bool bindScriptToMouseList( Uint8 button, const char* response, MouseButtonBindings* bindingsList )
{
	size_t responseLen = SDL_strlen( response );
	if( responseLen == 0 ) {
		llog( LOG_DEBUG, "Attempting to bind a script to mouse button with no reponse." );
		return false;
	}

	if( responseLen >= SCRIPT_FUNC_NAME_MAX_LEN ) {
		llog( LOG_DEBUG, "Attempting to bind a script to mouse button with a reponse with too long a name." );
		return false;
	}

	int idx;
	for( idx = 0; ( idx < MAX_BINDINGS ) && ( bindingsList[idx].response.type != CBT_NONE ); ++idx )
		;

	if( idx >= MAX_BINDINGS ) {
		llog( LOG_DEBUG, "Mouse button binding list full, increase size of bind list." );
		return false;
	}

	bindingsList[idx].button = button;
	bindingsList[idx].response.type = CBT_LUA;
	SDL_strlcpy( bindingsList[idx].response.scriptResponse.response, response, SCRIPT_FUNC_NAME_MAX_LEN );

	return true;
}

// Binds a function response when a key is pressed down.
//  Returns < 0 if there was a problem binding the key.
bool input_BindOnMouseButtonPress( Uint8 button, KeyResponse response )
{
	return bindToMouseList( button, response, mouseButtonDownBindings );
}

bool input_BindScriptOnMouseButtonPress( Uint8 button, const char* response )
{
	return bindScriptToMouseList( button, response, mouseButtonDownBindings );
}

// Binds a function response when a key is released.
//  Returns < 0 if there was a problem binding the key.
bool input_BindOnMouseButtonRelease( Uint8 button, KeyResponse response )
{
	return bindToMouseList( button, response, mouseButtonUpBindings );
}

bool input_BindScriptOnMouseButtonRelease( Uint8 button, const char* response )
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

// Clears all binded swipes.
void input_ClearAllSwipeBinds( void )
{
	for( int i = 0; i < MAX_BINDINGS; ++i ) {
		swipeBindings[i].response = NULL;
	}
}

// Binds a function response when a swipe is made in a specific direction.
//  Returns < 0 if there was a problem binding the swipe.
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
// Process events.
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