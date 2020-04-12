#ifndef CHECK_BOX_H
#define CHECK_BOX_H

#include <stdbool.h>
#include "../Math/vector2.h"
#include <SDL_events.h>

typedef void (*CheckBoxResponse)(bool);

/* Call this before trying to use any check boxes. */
void chkBox_Init( );

void chkBox_CleanUp( );

/*
Creates a button. All image ids must be valid.
*/
int chkBox_Create( Vector2 position, Vector2 size, const char* text, int fontID, int normalImg, int checkMarkImg,
	unsigned int camFlags, char depth, CheckBoxResponse response );
void chkBox_Destroy( int id );
void chkBox_DestroyAll( void );

bool chkBox_IsChecked( int id );
void chkBox_SetChecked( int id, bool val, bool respond );


#endif /* inclusion guard */