#include "sprites.h"
#include "images.h"
#include "color.h"
#include "../System/systems.h"
#include <assert.h>
#include <SDL.h>

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
			/*img_Draw_c_r( sprites[i].image, sprites[i].camFlags, sprites[i].oldState.pos, sprites[i].newState.pos,
				sprites[i].oldState.col, sprites[i].newState.col, sprites[i].oldState.rot, sprites[i].newState.rot, sprites[i].depth );*/
			sprites[i].oldState = sprites[i].newState;
		}
	}
}

int spr_Create( int image, unsigned int camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, char depth )
{
	int idx;
	for( idx = 0; ( idx < NUM_SPRITES ) && ( sprites[idx].image != -1 ); ++idx ) ;
	if( idx >= NUM_SPRITES ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Failed to create sprite, storage full.");
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

void spr_Update( int sprite, const Vector2* newPos, const Vector2* newScale, float newRot )
{
	assert( newPos != NULL );
	assert( newScale != NULL );

	sprites[sprite].newState.pos = *newPos;
	sprites[sprite].newState.rot = newRot;
	sprites[sprite].newState.scale = *newScale;
}

void spr_UpdateDelta( int sprite, const Vector2* posOffset, const Vector2* scaleOffset, float rotOffset )
{
	assert( posOffset != NULL );
	assert( scaleOffset != NULL );

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
}