/*
A simple wrapper that allows us to more quickly create entities in an ecps that can be rendered.
 For right now it will use it's own ECPS as this is meant for speeding up prototyping. In actual
 development you'd want to have the rendering be part of the standard ECPS being used in the game.
 This will let use just work with arrays of data directly instead of going through the ECPS which
 has been a point of lag in Ludum Dare.
*/

#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include "../Math/vector2.h"
#include "color.h"

#include "../System/ECPS/ecps_dataTypes.h"

// returns -1 is there was a problem
int spr_Init( void );
void spr_CleanUp( void );

EntityID spr_CreateSprite( int image, uint32_t camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, int8_t depth );
void spr_DestroySprite( EntityID sprite );

void spr_Update( EntityID sprite, const Vector2* newPos, const Vector2* newScale, float newRot );
void spr_Update_p( EntityID sprite, const Vector2* newPos );
void spr_Update_pc( EntityID sprite, const Vector2* newPos, const Color* clr );
void spr_Update_c( EntityID sprite, const Color* clr );
void spr_Update_sc( EntityID sprite, const Vector2* newScale, const Color* clr );
void spr_Update_psc( EntityID sprite, const Vector2* newPos, const Vector2* newScale, const Color* clr );
void spr_UpdateDelta( EntityID sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset );

#endif // inclusion guard