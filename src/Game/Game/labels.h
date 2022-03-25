#ifndef LABELS_H
#define LABELS_H

// Just some basic helping functions to easily draw persistent text.

#include "../UI/text.h"

void drawLabels( );
void createLabel( const char* utf8Str, Vector2 pos, Color clr, HorizTextAlignment hAlign, VertTextAlignment vAlign,
	int fontID, float fontSize, int camFlags, int8_t depth );
void destroyAllLabels( void );

#endif