/*
This will be used to more easily store and process the states of the images.
*/

#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include "../Math/vector2.h"
#include "color.h"

void spr_Init( void );
void spr_Draw( void );
int spr_Create( int image, uint32_t camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, int8_t depth );
void spr_Destroy( int sprite );

void spr_GetColor( int sprite, Color* outCol );
void spr_SetColor( int sprite, Color* col );

void spr_GetPosition( int sprite, Vector2* outPos );

void spr_Update( int sprite, const Vector2* newPos, const Vector2* newScale, float newRot );
void spr_Update_p( int sprite, const Vector2* newPos );
void spr_Update_pc( int sprite, const Vector2* newPos, const Color* clr );
void spr_Update_c( int sprite, const Color* clr );
void spr_Update_sc( int sprite, const Vector2* newScale, const Color* clr );
void spr_Update_psc( int sprite, const Vector2* newPos, const Vector2* newScale, const Color* clr );
void spr_UpdateDelta( int sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset );

/*
Handles the system level stuff.
 Returns a number < 0 if there was a problem.
*/
int spr_RegisterSystem( void );
void spr_UnRegisterSystem( void );

#endif // inclusion guard