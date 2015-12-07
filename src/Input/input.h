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

/***** Mouse Position Input *****/
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
*/
int input_GetMousePostion( Vector2* out );

/***** General Usage Functions *****/
/*
Process events.
*/
void input_ProcessEvents( SDL_Event* e );

#endif /* inclusion guard */