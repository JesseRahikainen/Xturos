/*
This will be used to more easily store and process the states of the images.
*/

#ifndef SPRITES_H
#define SPRITES_H

#include "../Math/vector2.h"
#include "color.h"

void spr_Init( void );
void spr_Draw( void );
int spr_Create( int image, unsigned int camFlags, Vector2 pos, Vector2 scale, float rot, Color col, char depth );
void spr_Destroy( int sprite );

void spr_GetColor( int sprite, Color* outCol );
void spr_SetColor( int sprite, Color* col );

void spr_Update( int sprite, const Vector2* newPos, const Vector2* newScale, float newRot );
void spr_UpdateDelta( int sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset );

#endif // inclusion guard