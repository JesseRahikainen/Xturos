#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL_events.h>

#include "Math/vector2.h"

bool input_Init( void );

/***** Key Binding *****/
typedef void (*KeyResponse)(void);

// Clears all the current key bindings.
void input_ClearAllKeyBinds( void );

// Clears all key bindings assocated with the passed in key code.
void input_ClearKeyBinds( SDL_Keycode code );

// Clears all key bindings associated with the passed in response function.
void input_ClearKeyResponse( KeyResponse response );

// Binds a function response when a key is pressed down.
//  Returns < 0 if there was a problem binding the key.
int input_BindOnKeyPress( SDL_Keycode code, KeyResponse response );
int input_BindScriptOnKeyPress( SDL_Keycode code, const char* func );

// Binds a function response when a key is released.
//  Returns < 0 if there was a problem binding the key.
int input_BindOnKeyRelease( SDL_Keycode code, KeyResponse response );
int input_BindScriptOnKeyRelease( SDL_Keycode code, const char* func );

// Binds function response when a key is pressed and released.
//  Returns < 0 if there was a problem binding the key.
int input_BindOnKey( SDL_Keycode code, KeyResponse onPressResponse, KeyResponse onReleaseResponse );
int input_BindScriptOnKey( SDL_Keycode code, const char* onPressFunc, const char* onReleaseFunc );

// Gets the code associated with response function. Puts them into the keyCodes array (which should be no larger than maxKeyCodes).
//  The rest of the array is filled with SDLK_UNKNOWN.
void input_GetKeyPressBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes );
void input_GetKeyReleaseBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes );

/***** Mouse Input *****/
// Sets up the coordinates for relative mouse position handling. Should be called after, and is only valid after, any changes to the render area or window size.
void input_SetupMouseArea( void );

// returns if the mouse is inside the window
bool input_MousePositionValid( void );

// Gets the relative position of the mouse.
//  Returns 1 if the position is inside the input area.
bool input_GetMousePosition( Vector2* out );

// Gets the position of the mouse in the window.
void input_GetRawMousePosition( Vector2* out );

// Get the world position relative to a camera
bool input_GetCamMousePos( int camera, Vector2* out );

// Clears all current mouse button bindings.
void input_ClearAllMouseButtonBinds( void );

// Clears all mouse button bindings associated with the passed in button code.
void input_ClearMouseButtonBinds( Uint8 button );

// Clears all mouse button bindsings associated with the passed in response.
void input_ClearMouseButtonReponse( KeyResponse response );

// Binds a function response when a key is pressed down.
//  Returns < 0 if there was a problem binding the key.
bool input_BindOnMouseButtonPress( Uint8 button, KeyResponse response );
bool input_BindScriptOnMouseButtonPress( Uint8 button, const char* response );

// Binds a function response when a key is released.
//  Returns < 0 if there was a problem binding the key.
bool input_BindOnMouseButtonRelease( Uint8 button, KeyResponse response );
bool input_BindScriptOnMouseButtonRelease( Uint8 button, const char* response );


/***** Gesture Recognition *****/
// This pulls from the mouse position (and by proxy the primary touch as well)

// Clears all binded swipes.
void input_ClearAllSwipeBinds( void );

// Binds a function response when a swipe is made in a specific direction.
//  Returns < 0 if there was a problem binding the swipe.
int input_BindOnSwipe( Vector2 dir, KeyResponse response );

/***** General Usage Functions *****/
// Process events.
void input_ProcessEvents( SDL_Event* e );

#endif /* inclusion guard */