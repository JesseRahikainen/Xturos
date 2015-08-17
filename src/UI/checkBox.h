#ifndef CHECK_BOX_H
#define CHECK_BOX_H

#include "../Math/Vector2.h"
#include <SDL_events.h>

typedef void (*CheckBoxResponse)(int);

/* Call this before trying to use any check boxes. */
void chkBox_Init( );

/*
Creates a button. All image ids must be valid.
*/
int chkBox_Create( Vector2 position, Vector2 size, const char* label, int fontID, int normalImg, int focusedImg, int clickedImg,
	unsigned int camFlags, char layer, CheckBoxResponse response );
void chkBox_Destroy( int id );
void chkBox_DestroyAll( void );

int chkBox_IsChecked( int id );

void chkBox_Draw( void );
void chkBox_Process( void );
void chkBox_ProcessEvents( SDL_Event* sdlEvent );


#endif /* inclusion guard */