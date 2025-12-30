#include "sprites.h"

#include <SDL3/SDL_assert.h>

#include "images.h"
#include "color.h"
#include "System/systems.h"
#include "System/platformLog.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/generalComponents.h"
#include "Utils/helpers.h"
#include "Graphics/graphics.h"

// Possibly improve this by putting all the storage into a separate thing, so we can have multiple sets of sprites we could draw
//  at different times without having to create and destroy them constantly, if we start needing something like that

static ECPS spriteECPS;

static ComponentID transformCompID = INVALID_COMPONENT_ID;
static ComponentID clrCompID = INVALID_COMPONENT_ID;
static ComponentID spriteCompID = INVALID_COMPONENT_ID;
static ComponentID floatVal0CompID = INVALID_COMPONENT_ID;
static ComponentID stencilCompID = INVALID_COMPONENT_ID;

static Process renderProc;
static Process swapProc;

static bool initialized = false;

static void runRenderProc( ECPS* ecps, const Entity* entity )
{
	gp_GeneralRender( ecps, entity, transformCompID, spriteCompID, clrCompID, floatVal0CompID, stencilCompID );
}

static void draw( float t )
{
	ecps_RunProcess( &spriteECPS, &renderProc );
}

static void runSwapProc( ECPS* ecps, const Entity* entity )
{
	gp_GeneralSwap( ecps, entity, transformCompID, clrCompID, floatVal0CompID );
}

static void swap( void )
{
	ecps_RunProcess( &spriteECPS, &swapProc );
}

int spr_Init( void )
{
	ASSERT_AND_IF_NOT( !initialized ) {
		return -1;
	}

	ecps_StartInitialization( &spriteECPS ); {
		// we're not concerned about serializing these as it's meant to just speed up prototyping
		transformCompID = ecps_AddComponentType( &spriteECPS, "TRNSFRM", 0, sizeof( GCTransformData ), ALIGN_OF( GCTransformData ), NULL, NULL, NULL );
		spriteCompID = ecps_AddComponentType( &spriteECPS, "SPRT", 0, sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL, NULL );
		clrCompID = ecps_AddComponentType( &spriteECPS, "CLR", 0, sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL, NULL );
		floatVal0CompID = ecps_AddComponentType( &spriteECPS, "VAL0", 0, sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL, NULL );
		stencilCompID = ecps_AddComponentType( &spriteECPS, "STNCL", 0, sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL, NULL );

		ecps_CreateProcess( &spriteECPS, "DRAW", NULL, runRenderProc, NULL, &renderProc, 2, transformCompID, spriteCompID );
		ecps_CreateProcess( &spriteECPS, "SWAP", NULL, runSwapProc, NULL, &swapProc, 1, transformCompID );
	} ecps_FinishInitialization( &spriteECPS );

	gfx_AddClearCommand( swap );
	gfx_AddDrawTrisFunc( draw );

	initialized = true;

	return 0;
}

void spr_CleanUp( void )
{
	gfx_RemoveClearCommand( swap );
	gfx_RemoveDrawTrisFunc( draw );

	ecps_DestroyAllEntities( &spriteECPS );
	ecps_CleanUp( &spriteECPS );

	initialized = false;
}

EntityID spr_CreateSprite( int image, uint32_t camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, int8_t depth )
{
	// we'll assume entities have every component type
	GCTransformData tfData = gc_CreateTransformPosRotScale( pos, rotRad, scale );

	GCSpriteData spriteData;
	spriteData.camFlags = camFlags;
	spriteData.depth = depth;
	spriteData.img = image;

	GCColorData colorData;
	colorData.currClr = col;
	colorData.futureClr = col;

	return ecps_CreateEntity( &spriteECPS, 3,
		transformCompID, &tfData,
		spriteCompID, &spriteData,
		clrCompID, &colorData );
}

void spr_DestroySprite( EntityID sprite )
{
	ecps_DestroyEntityByID( &spriteECPS, sprite );
}

void spr_Update( EntityID sprite, const Vector2* newPos, const Vector2* newScale, float newRot )
{
	SDL_assert( newPos != NULL );
	SDL_assert( newScale != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->futureState.pos = *newPos;
	tf->futureState.rotRad = newRot;
	tf->futureState.scale = *newScale;
}

void spr_SwitchImage( EntityID sprite, int newImage )
{
	SDL_assert( img_IsValidImage( newImage ) );
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCSpriteData* spriteData = NULL;
	ecps_GetComponentFromEntity( &entity, spriteCompID, &spriteData );
	spriteData->img = newImage;
}

int spr_GetImage( EntityID sprite )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return -1;
	}

	GCSpriteData* spriteData = NULL;
	ecps_GetComponentFromEntity( &entity, spriteCompID, &spriteData );
	return spriteData->img;
}

void spr_UpdatePos( EntityID sprite, const Vector2* newPos )
{
	SDL_assert( newPos != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->futureState.pos = *newPos;
}

void spr_UpdateColor( EntityID sprite, const Color* clr )
{
	SDL_assert( clr != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCColorData* color = NULL;
	ecps_GetComponentFromEntity( &entity, clrCompID, &color );
	color->futureClr = *clr;
}

void spr_UpdateScale( EntityID sprite, const Vector2* newScale )
{
	SDL_assert( newScale != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->futureState.scale = *newScale;
}

void spr_UpdateRot( EntityID sprite, float newRot )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->futureState.rotRad = newRot;
}

void spr_UpdatePos_Delta( EntityID sprite, const Vector2* posOffset )
{
	SDL_assert( posOffset != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	vec2_Add( &( tf->futureState.pos ), posOffset, &( tf->futureState.pos ) );
}

void spr_UpdateScale_Delta( EntityID sprite, const Vector2* scaleOffset )
{
	SDL_assert( scaleOffset != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	vec2_Add( &( tf->futureState.scale ), scaleOffset, &( tf->futureState.scale ) );
}

void spr_UpdateRot_Delta( EntityID sprite, float rotOffset )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->futureState.rotRad += rotOffset;
}

void spr_SnapPos( EntityID sprite, const Vector2* newPos )
{
	SDL_assert( newPos != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->currState.pos = *newPos;
	tf->futureState.pos = *newPos;
}

void spr_SnapCurrentPos( EntityID sprite )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCTransformData* tf = NULL;
	ecps_GetComponentFromEntity( &entity, transformCompID, &tf );
	tf->currState.pos = tf->futureState.pos;
}