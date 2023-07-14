#include "generalComponents.h"

#include <SDL_assert.h>
#include "Utils/helpers.h"
#include "System/platformLog.h"

ComponentID gcPosCompID = INVALID_COMPONENT_ID;
ComponentID gcClrCompID = INVALID_COMPONENT_ID;
ComponentID gcRotCompID = INVALID_COMPONENT_ID;
ComponentID gcScaleCompID = INVALID_COMPONENT_ID;
ComponentID gcSpriteCompID = INVALID_COMPONENT_ID;
ComponentID gcStencilCompID = INVALID_COMPONENT_ID;
ComponentID gc3x3SpriteCompID = INVALID_COMPONENT_ID;
ComponentID gcColliderCompID = INVALID_COMPONENT_ID;
ComponentID gcPointerResponseCompID = INVALID_COMPONENT_ID;
ComponentID gcCleanUpFlagCompID = INVALID_COMPONENT_ID;
ComponentID gcTextCompID = INVALID_COMPONENT_ID;
ComponentID gcFloatVal0CompID = INVALID_COMPONENT_ID;
ComponentID gcWatchCompID = INVALID_COMPONENT_ID;
ComponentID gcMountedPosOffsetCompID = INVALID_COMPONENT_ID;
ComponentID gcAlphaTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcPosTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcScaleTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcGroupIDCompID = INVALID_COMPONENT_ID;

// attaches the child entity to the parent entity, use the existing positions to calculate the offset
void gc_MountEntity( ECPS* ecps, EntityID parentID, EntityID childID )
{
	if( childID == INVALID_ENTITY_ID ) return;
	if( parentID == INVALID_ENTITY_ID ) return;

	Entity parent;
	Entity child;
	if( !ecps_GetEntityByID( ecps, parentID, &parent ) ) {
		llog( LOG_DEBUG, "Unable to find parent when mounting." );
		return;
	}
	if( !ecps_GetEntityByID( ecps, childID, &child ) ) {
		llog( LOG_DEBUG, "Unable to find child when mounting." );
		return;
	}

	GCPosData* parentPos = NULL;
	GCPosData* childPos = NULL;

	if( !ecps_GetComponentFromEntity( &parent, gcPosCompID, &parentPos ) ) {
		return;
	}

	if( !ecps_GetComponentFromEntity( &child, gcPosCompID, &childPos ) ) {
		return;
	}

	GCMountedPosOffset mountedOffset;
	vec2_Subtract( &( childPos->currPos ), &( parentPos->currPos) , &( mountedOffset.offset ) );
	mountedOffset.parentID = parentID;

	ecps_AddComponentToEntity( ecps, &child, gcMountedPosOffsetCompID, &mountedOffset );
}

void gc_WatchEntity( ECPS* ecps, EntityID id )
{
	if( id == INVALID_ENTITY_ID ) return;
	ecps_AddComponentToEntityByID( ecps, id, gcWatchCompID, NULL );
}

void gc_ColliderDataToCollider( GCColliderData* colliderData, GCPosData* posData, Collider* outCollider )
{
	SDL_assert( colliderData != NULL );
	SDL_assert( outCollider != NULL );

	switch( colliderData->base.type ) {
	case CT_DEACTIVATED:
		outCollider->type = CT_DEACTIVATED;
		break;
	case CT_AABB:
		outCollider->type = CT_AABB;
		outCollider->aabb.halfDim = colliderData->aab.halfDim;
		outCollider->aabb.center = posData->futurePos;
		break;
	case CT_CIRCLE:
		outCollider->type = CT_CIRCLE;
		outCollider->circle.radius = colliderData->circle.radius;
		outCollider->circle.center = posData->futurePos;
		break;
	case CT_HALF_SPACE:
		outCollider->type = CT_HALF_SPACE;
		collision_CalculateHalfSpace( &(posData->futurePos), &(colliderData->halfSpace.normal), outCollider );
		break;
	default:
		SDL_assert( false && "Invalid collider type." );
	}
}

void gc_Register( ECPS* ecps )
{
	gcPosCompID = ecps_AddComponentType( ecps, "GC_POS", 0, sizeof( GCPosData ), ALIGN_OF( GCPosData ), NULL, NULL );
	gcClrCompID = ecps_AddComponentType( ecps, "GC_CLR", 0, sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL );
	gcRotCompID = ecps_AddComponentType( ecps, "GC_ROT", 0, sizeof( GCRotData ), ALIGN_OF( GCRotData ), NULL, NULL );
	gcScaleCompID = ecps_AddComponentType( ecps, "GC_SCL", 0, sizeof( GCScaleData ), ALIGN_OF( GCScaleData ), NULL, NULL );
	gcSpriteCompID = ecps_AddComponentType( ecps, "GC_SPRT", 0, sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL );
	gc3x3SpriteCompID = ecps_AddComponentType( ecps, "GC_3x3", 0, sizeof( GC3x3SpriteData ), ALIGN_OF( GC3x3SpriteData ), NULL, NULL );
	gcColliderCompID = ecps_AddComponentType( ecps, "GC_COLL", 0, sizeof( GCColliderData ), ALIGN_OF( GCColliderData ), NULL, NULL );
	gcPointerResponseCompID = ecps_AddComponentType( ecps, "GC_CLICK", 0, sizeof( GCPointerResponseData ), ALIGN_OF( GCPointerResponseData ), NULL, NULL );
	gcCleanUpFlagCompID = ecps_AddComponentType( ecps, "GC_DEAD", 0, 0, 0, NULL, NULL );
	gcTextCompID = ecps_AddComponentType( ecps, "GC_TXT", 0, sizeof( GCTextData ), ALIGN_OF( GCTextData ), NULL, NULL );
	gcFloatVal0CompID = ecps_AddComponentType( ecps, "GC_VAL0", 0, sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL );
	gcWatchCompID = ecps_AddComponentType( ecps, "GC_WATCH", 0, 0, 0, NULL, NULL );
	gcMountedPosOffsetCompID = ecps_AddComponentType( ecps, "GC_MNTPOS", 0, sizeof( GCMountedPosOffset ), ALIGN_OF( GCMountedPosOffset ), NULL, NULL );
	gcStencilCompID = ecps_AddComponentType( ecps, "GC_STENCIL", 0, sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL );

	gcPosTweenCompID = ecps_AddComponentType( ecps, "P_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL );
	gcScaleTweenCompID = ecps_AddComponentType( ecps, "S_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL );
	gcAlphaTweenCompID = ecps_AddComponentType( ecps, "A_TWN", 0, sizeof( GCFloatTweenData ), ALIGN_OF( GCFloatTweenData ), NULL, NULL );

	gcGroupIDCompID = ecps_AddComponentType( ecps, "GRP", 0, sizeof( GCGroupIDData ), ALIGN_OF( GCGroupIDData ), NULL, NULL );
}
