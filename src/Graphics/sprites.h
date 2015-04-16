/*
This will be used to more easily store and process the states of the images.
*/

#ifndef SPRITES_H
#define SPRITES_H

#include "../Math/vector2.h"
#include "color.h"

void initSprites( void );
void drawSprites( void );
int createSprite( int image, unsigned int camFlags, Vector2 pos, Vector2 scale, float rot, Color col, char depth );
void destroySprite( int sprite );
void updateSprite( int sprite, const Vector2* newPos, const Vector2* newScale, float newRot );
void updateSprite_Add( int sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset );

#endif // inclusion guard