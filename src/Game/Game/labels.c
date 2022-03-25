#include "labels.h"

#include "../Utils/stretchyBuffer.h"

typedef struct {
	char* str;
	Vector2 pos;
	Color clr;
	HorizTextAlignment hAlign;
	VertTextAlignment vAlign;
	int fontID;
	float fontSize;
	int camFlags;
	int8_t depth;
} Label;

Label* sbLabels = NULL;

void drawLabels( )
{
	for( size_t i = 0; i < sb_Count( sbLabels ); ++i ) {
		Label* lbl = &( sbLabels[i] );
		txt_DisplayString( lbl->str, lbl->pos, lbl->clr, lbl->hAlign, lbl->vAlign, lbl->fontID, lbl->camFlags, lbl->depth, lbl->fontSize );
	}
}

void createLabel( const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, float fontSize, int camFlags, int8_t depth )
{
	Label* newLabel = sb_Add( sbLabels, 1 );

	// create a copy of the string
	size_t len = SDL_strlen( utf8Str );
	newLabel->str = mem_Allocate( len + 1 );
	SDL_strlcpy( newLabel->str, utf8Str, len + 1 );

	//newLabel->str = utf8Str;
	newLabel->pos = pos;
	newLabel->clr = clr;
	newLabel->hAlign = hAlign;
	newLabel->vAlign = vAlign;
	newLabel->fontID = fontID;
	newLabel->fontSize = fontSize;
	newLabel->camFlags = camFlags;
	newLabel->depth = depth;
}

void destroyAllLabels( void )
{
	for( size_t i = 0; i < sb_Count( sbLabels ); ++i ) {
		mem_Release( sbLabels[i].str );
	}
	sb_Clear( sbLabels );
}