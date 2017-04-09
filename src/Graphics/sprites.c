#include "sprites.h"

#include <assert.h>

#include "images.h"
#include "color.h"
#include "../System/systems.h"
#include "../System/platformLog.h"

// Possibly improve this by putting all the storage into a separate thing, so we can have multiple sets of sprites we could draw
//  at different times without having to create and destroy them constantly, if we start needing something like that

typedef struct {
	Vector2 pos;
	float rot;
	Color col;
	Vector2 scale;
} SpriteState;

typedef struct {
	int image;

	SpriteState oldState;
	SpriteState newState;

	uint32_t camFlags;
	int8_t depth;
} Sprite;

#define NUM_SPRITES 1000
static Sprite sprites[NUM_SPRITES];

static int systemID = -1;

#define SPRITE_ID_VALID( id ) ( ( ( id ) >= 0 ) && ( ( id ) < NUM_SPRITES ) )

void spr_Init( void )
{
	for( int i = 0; i < NUM_SPRITES; ++i ) {
		sprites[i].image = -1;
	}
}

void spr_Draw( void )
{
	for( int i = 0; i < ( sizeof( sprites ) / sizeof( sprites[0] ) ); ++i ) {
		if( sprites[i].image != -1 ) {
			img_Draw_sv_c_r( sprites[i].image, sprites[i].camFlags, sprites[i].oldState.pos, sprites[i].newState.pos,
				sprites[i].oldState.scale, sprites[i].newState.scale, sprites[i].oldState.col, sprites[i].newState.col,
				sprites[i].oldState.rot, sprites[i].newState.rot, sprites[i].depth );
			sprites[i].oldState = sprites[i].newState;
		}
	}
}

int spr_Create( int image, uint32_t camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, int8_t depth )
{
	int idx;
	for( idx = 0; ( idx < NUM_SPRITES ) && ( sprites[idx].image != -1 ); ++idx ) ;
	if( idx >= NUM_SPRITES ) {
		llog( LOG_DEBUG, "Failed to create sprite, storage full.");
		return -1;
	}

	sprites[idx].image = image;
	sprites[idx].depth = depth;
	sprites[idx].newState.pos = pos;
	sprites[idx].newState.scale = scale;
	sprites[idx].newState.rot = rotRad;
	sprites[idx].newState.col = col;
	sprites[idx].oldState = sprites[idx].newState;
	sprites[idx].camFlags = camFlags;

	return idx;
}

void spr_Destroy( int sprite )
{
	sprites[sprite].image = -1;
}

void spr_GetColor( int sprite, Color* outCol )
{
	(*outCol) = sprites[sprite].newState.col;
}

void spr_SetColor( int sprite, Color* col )
{
	sprites[sprite].newState.col = *col;
}

void spr_GetPosition( int sprite, Vector2* outPos )
{
	(*outPos) = sprites[sprite].oldState.pos;
}

void spr_Update( int sprite, const Vector2* newPos, const Vector2* newScale, float newRot )
{
	assert( newPos != NULL );
	assert( newScale != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.pos = *newPos;
	sprites[sprite].newState.rot = newRot;
	sprites[sprite].newState.scale = *newScale;
}

void spr_Update_p( int sprite, const Vector2* newPos )
{
	assert( newPos != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.pos = *newPos;
}

void spr_Update_pc( int sprite, const Vector2* newPos, const Color* clr )
{
	assert( newPos != NULL );
	assert( clr != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.pos = *newPos;
	sprites[sprite].newState.col = *clr;
}

void spr_Update_c( int sprite, const Color* clr )
{
	assert( clr != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.col = *clr;
}

void spr_Update_sc( int sprite, const Vector2* newScale, const Color* clr )
{
	assert( newScale != NULL );
	assert( clr != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.col = *clr;
	sprites[sprite].newState.scale = *newScale;
}

void spr_Update_psc( int sprite, const Vector2* newPos, const Vector2* newScale, const Color* clr )
{
	assert( newPos != NULL );
	assert( newScale != NULL );
	assert( clr != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	sprites[sprite].newState.pos = *newPos;
	sprites[sprite].newState.scale = *newScale;
	sprites[sprite].newState.col = *clr;
}

void spr_UpdateDelta( int sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset )
{
	assert( posOffset != NULL );
	assert( scaleOffset != NULL );

	if( !SPRITE_ID_VALID( sprite ) ) return;

	vec2_Add( &( sprites[sprite].newState.pos ), posOffset, &( sprites[sprite].newState.pos ) );
	vec2_Add( &( sprites[sprite].newState.scale ), scaleOffset, &( sprites[sprite].newState.scale ) );
	sprites[sprite].newState.rot += rotOffset;
}

int spr_RegisterSystem( void )
{
	systemID = sys_Register( NULL, NULL, spr_Draw, NULL );
	return systemID;
}

void spr_UnRegisterSystem( void )
{
	sys_UnRegister( systemID );
	systemID = -1;
}