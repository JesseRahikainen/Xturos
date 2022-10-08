#include "sprites.h"

#include <assert.h>

#include "images.h"
#include "color.h"
#include "System/systems.h"
#include "System/platformLog.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "Processes/generalProcesses.h"
#include "Components/generalComponents.h"
#include "Utils/helpers.h"

// Possibly improve this by putting all the storage into a separate thing, so we can have multiple sets of sprites we could draw
//  at different times without having to create and destroy them constantly, if we start needing something like that

static ECPS spriteECPS;

static ComponentID posCompID = INVALID_COMPONENT_ID;
static ComponentID clrCompID = INVALID_COMPONENT_ID;
static ComponentID rotCompID = INVALID_COMPONENT_ID;
static ComponentID scaleCompID = INVALID_COMPONENT_ID;
static ComponentID spriteCompID = INVALID_COMPONENT_ID;
static ComponentID floatVal0CompID = INVALID_COMPONENT_ID;
static ComponentID stencilCompID = INVALID_COMPONENT_ID;

static Process renderProc;

static int systemID = -1;

static void render( ECPS* ecps, const Entity* entity )
{
	gp_GeneralRender( ecps, entity, posCompID, spriteCompID, scaleCompID, clrCompID, rotCompID, floatVal0CompID, stencilCompID );
}

static void draw( void )
{
	ecps_RunProcess( &spriteECPS, &renderProc );
}

int spr_Init( void )
{
	ecps_StartInitialization( &spriteECPS ); {
		posCompID = ecps_AddComponentType( &spriteECPS, "POS", sizeof( GCPosData ), ALIGN_OF( GCPosData ), NULL, NULL );
		spriteCompID = ecps_AddComponentType( &spriteECPS, "SPRT", sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL );
		scaleCompID = ecps_AddComponentType( &spriteECPS, "SCL", sizeof( GCScaleData ), ALIGN_OF( GCScaleData ), NULL, NULL );
		clrCompID = ecps_AddComponentType( &spriteECPS, "CLR", sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL );
		rotCompID = ecps_AddComponentType( &spriteECPS, "ROT", sizeof( GCRotData ), ALIGN_OF( GCRotData ), NULL, NULL );
		floatVal0CompID = ecps_AddComponentType( &spriteECPS, "VAL0", sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL );
		stencilCompID = ecps_AddComponentType( &spriteECPS, "STNCL", sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL );

		ecps_CreateProcess( &spriteECPS, "DRAW", NULL, render, NULL, &renderProc, 2, posCompID, spriteCompID );
	} ecps_FinishInitialization( &spriteECPS );

	systemID = sys_Register( NULL, NULL, draw, NULL );

	if( systemID < 0 ) {
		return -1;
	}

	return 0;
}

void spr_CleanUp( void )
{
	sys_UnRegister( systemID );
	systemID = -1;

	ecps_DestroyAllEntities( &spriteECPS );
	ecps_CleanUp( &spriteECPS );
}

EntityID spr_CreateSprite( int image, uint32_t camFlags, Vector2 pos, Vector2 scale, float rotRad, Color col, int8_t depth )
{
	// we'll assume entities have every component type
	GCPosData posData;
	posData.currPos = pos;
	posData.futurePos = pos;

	GCSpriteData spriteData;
	spriteData.camFlags = camFlags;
	spriteData.depth = depth;
	spriteData.img = image;

	GCScaleData scaleData;
	scaleData.currScale = scale;
	scaleData.futureScale = scale;

	GCColorData colorData;
	colorData.currClr = col;
	colorData.futureClr = col;

	GCRotData rotData;
	rotData.currRot = rotRad;
	rotData.futureRot = rotRad;

	return ecps_CreateEntity( &spriteECPS, 5,
		posCompID, &posData,
		spriteCompID, &spriteData,
		scaleCompID, &scaleData,
		clrCompID, &colorData,
		rotCompID, &rotData );
}

void spr_DestroySprite( EntityID sprite )
{
	ecps_DestroyEntityByID( &spriteECPS, sprite );
}

void spr_Update( EntityID sprite, const Vector2* newPos, const Vector2* newScale, float newRot )
{
	assert( newPos != NULL );
	assert( newScale != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCPosData* pos = NULL;
	ecps_GetComponentFromEntity( &entity, posCompID, &pos );
	pos->futurePos = *newPos;

	GCScaleData* scale = NULL;
	ecps_GetComponentFromEntity( &entity, scaleCompID, &scale );
	scale->futureScale = *newScale;

	GCRotData* rot = NULL;
	ecps_GetComponentFromEntity( &entity, rotCompID, &rot );
	rot->futureRot = newRot;
}

void spr_SwitchImage( EntityID sprite, int newImage )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCSpriteData* spriteData = NULL;
	ecps_GetComponentFromEntity( &entity, spriteCompID, &spriteData );
	spriteData->img = newImage;
}

void spr_UpdatePos( EntityID sprite, const Vector2* newPos )
{
	assert( newPos != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCPosData* pos = NULL;
	ecps_GetComponentFromEntity( &entity, posCompID, &pos );
	pos->futurePos = *newPos;
}

void spr_UpdateColor( EntityID sprite, const Color* clr )
{
	assert( clr != NULL );

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
	assert( newScale != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCScaleData* scale = NULL;
	ecps_GetComponentFromEntity( &entity, scaleCompID, &scale );
	scale->futureScale = *newScale;
}

void spr_UpdateRot( EntityID sprite, float newRot )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCRotData* rot = NULL;
	ecps_GetComponentFromEntity( &entity, rotCompID, &rot );
	rot->futureRot = newRot;
}

void spr_UpdatePos_Delta( EntityID sprite, const Vector2* posOffset )
{
	assert( posOffset != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCPosData* pos = NULL;
	ecps_GetComponentFromEntity( &entity, posCompID, &pos );
	vec2_Add( &( pos->futurePos ), posOffset, &( pos->futurePos ) );
}

void spr_UpdateScale_Delta( EntityID sprite, const Vector2* scaleOffset )
{
	assert( scaleOffset != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCScaleData* scale = NULL;
	ecps_GetComponentFromEntity( &entity, scaleCompID, &scale );
	vec2_Add( &( scale->futureScale ), scaleOffset, &( scale->futureScale ) );
}

void spr_UpdateRot_Delta( EntityID sprite, float rotOffset )
{
	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCRotData* rot = NULL;
	ecps_GetComponentFromEntity( &entity, rotCompID, &rot );
	rot->futureRot += rotOffset;
}

void spr_SnapPos( EntityID sprite, const Vector2* newPos )
{
	assert( newPos != NULL );

	Entity entity;
	if( !ecps_GetEntityByID( &spriteECPS, sprite, &entity ) ) {
		return;
	}

	GCPosData* pos = NULL;
	ecps_GetComponentFromEntity( &entity, posCompID, &pos );
	pos->futurePos = *newPos;
	pos->currPos = *newPos;
}