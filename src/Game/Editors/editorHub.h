#ifndef EDITOR_HUB_H
#define EDITOR_HUB_H

#include "gameState.h"

typedef enum {
	DT_MESSAGE,
	DT_CHOICE,
	DT_WARNING,
	DT_ERROR,
	DIALOG_TYPE_COUNT
} DialogType;

void hub_CreateDialog( const char* title, const char* text, DialogType type, int numButtons, ... );
void hub_RegisterAllEditors( void );

extern GameState editorHubScreenState;

#endif
