#ifndef INPUT_H
#define INPUT_H

#include <SDL_events.h>

#include "../Math/vector2.h"

/***** Key Binding *****/
typedef void (*KeyResponse)(void);

/*
Clears all the current key bindings.
*/
void input_ClearAllKeyBinds( void );

/*
Clears all key bindings assocated with the passed in key code.
*/
void input_ClearKeyBinds( SDL_Keycode code );

/*
Clears all key bindings associated with the passed in response function.
*/
void input_ClearKeyResponse( KeyResponse response );

/*
Binds a function response when a key is pressed down.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnKeyPress( SDL_Keycode code, KeyResponse response );

/*
Binds a function response when a key is released.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnKeyRelease( SDL_Keycode code, KeyResponse response );

/*
Gets the code associated with response function. Puts them into the keyCodes array (which should be no larger than maxKeyCodes).
 The rest of the array is filled with SDLK_UNKNOWN.
*/
void input_GetKeyPressBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes );
void input_GetKeyReleaseBindings( KeyResponse response, SDL_Keycode* keyCodes, int maxKeyCodes );

/***** Mouse Input *****/
/*
Sets up the coordinates for relative mouse position handling.
*/
void input_InitMouseInputArea( int inputWidth, int inputHeight );

/*
Sets the size of the window, used for getting relative mouse position.
*/
void input_UpdateMouseWindow( int windowWidth, int windowHeight );

/*
Gets the relative position of the mouse.
 Returns 1 if the position is inside the input area.
*/
int input_GetMousePosition( Vector2* out );

/*
Gets the position of the mouse in the window.
*/
void input_GetRawMousePosition( Vector2* out );

/*
Clears all current mouse button bindings.
*/
void input_ClearAllMouseButtonBinds( void );

/*
Clears all mouse button bindings associated with the passed in button code.
*/
void input_ClearMouseButtonBinds( Uint8 button );

/*
Clears all mouse button bindsings associated with the passed in response.
*/
void input_ClearMouseButtonReponse( KeyResponse response );

/*
Binds a function response when a key is pressed down.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnMouseButtonPress( Uint8 button, KeyResponse response );

/*
Binds a function response when a key is released.
 Returns < 0 if there was a problem binding the key.
*/
int input_BindOnMouseButtonRelease( Uint8 button, KeyResponse response );


/***** Gesture Recognition *****/
// This pulls from the mouse position (and by proxy the primary touch as well)

/*
Clears all binded swipes.
*/
void input_ClearAllSwipeBinds( void );

/*
Binds a function response when a swipe is made in a specific direction.
 Returns < 0 if there was a problem binding the swipe.
*/
int input_BindOnSwipe( Vector2 dir, KeyResponse response );

/***** General Usage Functions *****/
/*
Process events.
*/
void input_ProcessEvents( SDL_Event* e );

#endif /* inclusion guard */