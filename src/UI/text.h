#ifndef TEXT_H
#define TEXT_H

#include "../Graphics/color.h"
#include "../Math/vector2.h"

typedef enum {
	HORIZ_ALIGN_LEFT,
	HORIZ_ALIGN_RIGHT,
	HORIZ_ALIGN_CENTER
} HorizTextAlignment;

typedef enum {
	VERT_ALIGN_BASE_LINE,
	VERT_ALIGN_BOTTOM,
	VERT_ALIGN_TOP,
	VERT_ALIGN_CENTER
} VertTextAlignment;

/*
Sets up the default codepoints to load and clears out any currently loaded fonts.
*/
int txt_Init( void );

/*
Adds a codepoint to the set of codepoints to get glyphs for when loading a font. This does
 not currently cause any currently loaded fonts to be reloaded, so they will not be able to
 display this codepoint if it had not previously been added.
*/
void txt_AddCharacterToLoad( int c );

/*
Loads the font at fileName, with a height of pixelHeight.
 Returns an ID to be used when displaying a string, returns -1 if there was an issue.
*/
int txt_LoadFont( const char* fileName, float pixelHeight );

/*
Frees up the font specified by fontID.
*/
void txt_UnloadFont( int fontID );

/*
Draws a string on the screen. The base line is determined by pos.
*/
void txt_DisplayString( const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, int camFlags, char depth );

#endif /* inclusion guard */