#include "generalComponents.h"

#include <SDL_assert.h>
#include "Utils/helpers.h"
#include "System/platformLog.h"
#include "System/ECPS/ecps_trackedCallbacks.h"
#include "System/ECPS/ecps_serialization.h"

#include "Graphics/images.h"

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

// serialization and deserialization functions
//====
static bool serializePosComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPosData* pos = (GCPosData*)data;

	if( !vec2_Serialize( cmp, &( pos->currPos ) ) ) {
		llog( LOG_ERROR, "Unable to serialize currPos of GCPosData." );
		return false;
	}

	if( !vec2_Serialize( cmp, &( pos->futurePos) ) ) {
		llog( LOG_ERROR, "Unable to serialize futurePos of GCPosData." );
		return false;
	}

	return true;
}

static bool deserializePosComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPosData* pos = (GCPosData*)data;

	if( !vec2_Deserialize( cmp, &( pos->currPos ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize currPos of GCPosData." );
		return false;
	}

	if( !vec2_Deserialize( cmp, &( pos->futurePos ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize futurePos of GCPosData." );
		return false;
	}

	return true;
}

//====
static bool serializeColorComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColorData* clr = (GCColorData*)data;

	if( !clr_Serialize( cmp, &( clr->currClr ) ) ) {
		llog( LOG_ERROR, "Unable to serialize currClr of GCColorData." );
		return false;
	}

	if( !clr_Serialize( cmp, &( clr->futureClr ) ) ) {
		llog( LOG_ERROR, "Unable to serialize futureClr of GCColorData." );
		return false;
	}

	return true;
}

static bool deserializeColorComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColorData* clr = (GCColorData*)data;

	if( !clr_Deserialize( cmp, &( clr->currClr ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize currClr of GCColorData." );
		return false;
	}

	if( !clr_Deserialize( cmp, &( clr->futureClr ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize futureClr of GCColorData." );
		return false;
	}

	return true;
}

//====
static bool serializeRotComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCRotData* rot = (GCRotData*)data;

	if( !cmp_write_float( cmp, rot->currRot ) ) {
		llog( LOG_ERROR, "Unable to serialize currRot of GCRotData." );
		return false;
	}

	if( !cmp_write_float( cmp, rot->futureRot ) ) {
		llog( LOG_ERROR, "Unable to serialize futureRot of GCRotData." );
		return false;
	}
	
	return true;
}

static bool deserializeRotComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCRotData* rot = (GCRotData*)data;

	if( !cmp_read_float( cmp, &( rot->currRot ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize currRot of GCRotData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( rot->futureRot ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize futureRot of GCRotData." );
		return false;
	}

	return true;
}

//====
static bool serializeScaleComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCScaleData* pos = (GCScaleData*)data;

	if( !vec2_Serialize( cmp, &( pos->currScale ) ) ) {
		llog( LOG_ERROR, "Unable to serialize currScale of GCScaleData." );
		return false;
	}

	if( !vec2_Serialize( cmp, &( pos->futureScale ) ) ) {
		llog( LOG_ERROR, "Unable to serialize futureScale of GCScaleData." );
		return false;
	}

	return true;
}

static bool deserializeScaleComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCScaleData* pos = (GCScaleData*)data;

	if( !vec2_Deserialize( cmp, &( pos->currScale ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize currScale of GCScaleData." );
		return false;
	}

	if( !vec2_Deserialize( cmp, &( pos->futureScale ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize futureScale of GCScaleData." );
		return false;
	}

	return true;
}

//====
static bool serializeSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSpriteData* sprite = (GCSpriteData*)data;

	if( !cmp_write_u32( cmp, sprite->camFlags ) ) {
		llog( LOG_ERROR, "Unable to serialize camFlags of GCSpriteData." );
		return false;
	}

	if( !cmp_write_s8( cmp, sprite->depth ) ) {
		llog( LOG_ERROR, "Unable to serialize depth of GCSpriteData." );
		return false;
	}

	const char* imgID = img_GetImgStringID( sprite->img );
	uint32_t len = (uint32_t)strlenNullTest( imgID );
	if( !cmp_write_str( cmp, imgID, len ) ) {
		llog( LOG_ERROR, "Unable to serialize img of GCSpriteData." );
		return false;
	}

	return true;
}

static bool deserializeSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSpriteData* sprite = (GCSpriteData*)data;

	if( !cmp_read_u32( cmp, &( sprite->camFlags ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize camFlags of GCSpriteData." );
		return false;
	}

	if( !cmp_read_s8( cmp, &( sprite->depth ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize depth of GCSpriteData." );
		return false;
	}

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );

	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for img of GCSpriteData." );
	} else {
		sprite->img = img_GetExistingByID( idBuffer );
		if( sprite->img == -1 ) {
			llog( LOG_ERROR, "Unable to find img with id %s for GCSpriteData.", idBuffer );
		}
	}

	return true;
}

//====
static bool serialize3x3Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GC3x3SpriteData* sprite = (GC3x3SpriteData*)data;

	if( !cmp_write_u32( cmp, sprite->camFlags ) ) {
		llog( LOG_ERROR, "Unable to serialize camFlags of GC3x3SpriteData." );
		return false;
	}

	if( !cmp_write_s8( cmp, sprite->depth ) ) {
		llog( LOG_ERROR, "Unable to serialize depth of GC3x3SpriteData." );
		return false;
	}

	for( size_t i = 0; i < ARRAY_SIZE( sprite->imgs ); ++i ) {
		const char* imgID = img_GetImgStringID( sprite->imgs[i] );
		uint32_t len = (uint32_t)strlenNullTest( imgID );
		if( !cmp_write_str( cmp, imgID, len ) ) {
			llog( LOG_ERROR, "Unable to serialize img of GC3x3SpriteData." );
			return false;
		}
	}

	return true;
}

static bool deserialize3x3Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GC3x3SpriteData* sprite = (GC3x3SpriteData*)data;

	if( !cmp_read_u32( cmp, &( sprite->camFlags ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize camFlags of GC3x3SpriteData." );
		return false;
	}

	if( !cmp_read_s8( cmp, &( sprite->depth ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize depth of GC3x3SpriteData." );
		return false;
	}

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );

	for( size_t i = 0; i < ARRAY_SIZE( sprite->imgs ); ++i ) {
		if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
			llog( LOG_ERROR, "Unable to deserialize id for img of GCSpriteData." );
		} else {
			sprite->imgs[i] = img_GetExistingByID( idBuffer );
			if( sprite->imgs[i] == -1 ) {
				llog( LOG_ERROR, "Unable to find img with id %s for GCSpriteData.", idBuffer );
			}
		}
	}

	return true;
}

//====
static bool serializeCollComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColliderData* coll = (GCColliderData*)data;

	if( !cmp_write_int( cmp, (int)coll->base.type ) ) {
		llog( LOG_ERROR, "Unable to serialize base type of GCColliderData." );
		return false;
	}

	switch( coll->base.type )
	{
	case CT_AABB:
		if( !vec2_Serialize( cmp, &( coll->aab.halfDim ) ) ) {
			llog( LOG_ERROR, "Unable to serialize halfDim of GCColliderData." );
			return false;
		}
		break;
	case CT_CIRCLE:
		if( cmp_write_float( cmp, coll->circle.radius ) ) {
			llog( LOG_ERROR, "Unable to serialize radius of GCColliderData." );
			return false;
		}
		break;
	case CT_HALF_SPACE:
		if( !vec2_Serialize( cmp, &( coll->halfSpace.normal ) ) ) {
			llog( LOG_ERROR, "Unable to serialize normal of GCColliderData." );
			return false;
		}
		break;
	}

	return true;
}

static bool deserializeCollComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColliderData* coll = (GCColliderData*)data;

	int32_t readType;
	if( !cmp_read_int( cmp, &readType ) ) {
		llog( LOG_ERROR, "Unable to deserialize base type of GCColliderData." );
		return false;
	} else {
		coll->base.type = (enum ColliderType)readType;
	}

	switch( coll->base.type )
	{
	case CT_AABB:
		if( !vec2_Deserialize( cmp, &( coll->aab.halfDim ) ) ) {
			llog( LOG_ERROR, "Unable to deserialize halfDim of GCColliderData." );
			return false;
		}
		break;
	case CT_CIRCLE:
		if( cmp_read_float( cmp, &(coll->circle.radius) ) ) {
			llog( LOG_ERROR, "Unable to deserialize radius of GCColliderData." );
			return false;
		}
		break;
	case CT_HALF_SPACE:
		if( !vec2_Deserialize( cmp, &( coll->halfSpace.normal ) ) ) {
			llog( LOG_ERROR, "Unable to deserialize normal of GCColliderData." );
			return false;
		}
		break;
	}

	return true;
}

//====
static bool serializeClickResponseComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerResponseData* click = (GCPointerResponseData*)data;

	if( !vec2_Serialize( cmp, &( click->collisionHalfDim ) ) ) {
		llog( LOG_ERROR, "Unable to serialize collisionHalfDim of GCPointerResponseData." );
		return false;
	}

	if( !cmp_write_uint( cmp, click->camFlags ) ) {
		llog( LOG_ERROR, "Unable to serialize camFlags of GCPointerResponseData." );
		return false;
	}

	if( !cmp_write_s32( cmp, click->priority ) ) {
		llog( LOG_ERROR, "Unable to serialize priority of GCPointerResponseData." );
		return false;
	}

	const char* overResponseID = ecps_GetTrackedIDFromECPSCallback( click->overResponse );
	uint32_t overResponseLen = (uint32_t)strlenNullTest( overResponseID );
	if( !cmp_write_str( cmp, overResponseID, overResponseLen ) ) {
		llog( LOG_ERROR, "Unable to serialize overResponse of GCPointerResponseData." );
		return false;
	}

	const char* leaveResponseID = ecps_GetTrackedIDFromECPSCallback( click->leaveResponse );
	uint32_t leaveResponseLen = (uint32_t)strlenNullTest( leaveResponseID );
	if( !cmp_write_str( cmp, leaveResponseID, leaveResponseLen ) ) {
		llog( LOG_ERROR, "Unable to serialize leaveResponse of GCPointerResponseData." );
		return false;
	}

	const char* pressResponseID = ecps_GetTrackedIDFromECPSCallback( click->pressResponse );
	uint32_t pressResponseLen = (uint32_t)strlenNullTest( pressResponseID );
	if( !cmp_write_str( cmp, pressResponseID, pressResponseLen ) ) {
		llog( LOG_ERROR, "Unable to serialize pressResponse of GCPointerResponseData." );
		return false;
	}

	const char* releaseResponseID = ecps_GetTrackedIDFromECPSCallback( click->releaseResponse );
	uint32_t releaseResponseLen = (uint32_t)strlenNullTest( releaseResponseID );
	if( !cmp_write_str( cmp, releaseResponseID, releaseResponseLen ) ) {
		llog( LOG_ERROR, "Unable to serialize releaseResponse of GCPointerResponseData." );
		return false;
	}

	return true;
}

static bool deserializeClickResponseComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerResponseData* click = (GCPointerResponseData*)data;

	if( !vec2_Deserialize( cmp, &( click->collisionHalfDim ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize collisionHalfDim of GCPointerResponseData." );
		return false;
	}

	if( !cmp_read_uint( cmp, &(click->camFlags) ) ) {
		llog( LOG_ERROR, "Unable to deserialize camFlags of GCPointerResponseData." );
		return false;
	}

	if( !cmp_read_s32( cmp, &(click->priority) ) ) {
		llog( LOG_ERROR, "Unable to deserialize priority of GCPointerResponseData." );
		return false;
	}

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );
	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for overResponse of GCPointerResponseData." );
		return false;
	} else {
		click->overResponse = ecps_GetTrackedECPSCallbackFromID( idBuffer );
	}

	bufferSize = ARRAY_SIZE( idBuffer );
	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for leaveResponse of GCPointerResponseData." );
		return false;
	} else {
		click->leaveResponse = ecps_GetTrackedECPSCallbackFromID( idBuffer );
	}

	bufferSize = ARRAY_SIZE( idBuffer );
	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for pressResponse of GCPointerResponseData." );
		return false;
	} else {
		click->pressResponse = ecps_GetTrackedECPSCallbackFromID( idBuffer );
	}

	bufferSize = ARRAY_SIZE( idBuffer );
	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for releaseResponse of GCPointerResponseData." );
		return false;
	} else {
		click->releaseResponse = ecps_GetTrackedECPSCallbackFromID( idBuffer );
	}

	return true;
}

//====
static bool serializeTextComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCTextData* text = (GCTextData*)data;

	// TODO: Need to be able to save out a font.
	SDL_assert_always( false && "serializeTextComp: Not implemented yet." );
	//uint32_t camFlags;
	//int8_t depth;
	//const uint8_t* text;
	//int fontID;
	//Color clr; // assuming any color components aren't for this, need to find a better way to do this
	//float pixelSize;

	return true;
}

static bool deserializeTextComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCTextData* text = (GCTextData*)data;

	// TODO: Need to be able to save out a font.
	SDL_assert_always( false && "deserializeTextComp: Not implemented yet." );

	return true;
}

//====
static bool serializeVal0Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatVal0Data* val = (GCFloatVal0Data*)data;

	if( !cmp_write_float( cmp, val->currValue ) ) {
		llog( LOG_ERROR, "Unable to serialize currValue of GCFloatVal0Data." );
		return false;
	}

	if( !cmp_write_float( cmp, val->futureValue ) ) {
		llog( LOG_ERROR, "Unable to serialize futureValue of GCFloatVal0Data." );
		return false;
	}

	return true;
}

static bool deserializeVal0Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatVal0Data* val = (GCFloatVal0Data*)data;

	if( !cmp_read_float( cmp, &( val->currValue ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize currValue of GCFloatVal0Data." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->futureValue ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize futureValue of GCFloatVal0Data." );
		return false;
	}


	return true;
}

//====
static bool serializeStencilComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCStencilData* val = (GCStencilData*)data;

	if( !cmp_write_bool( cmp, val->isStencil ) ) {
		llog( LOG_ERROR, "Unable to serialize isStencil of GCStencilData." );
		return false;
	}

	if( !cmp_write_int( cmp, val->stencilID ) ) {
		llog( LOG_ERROR, "Unable to serialize stencilID of GCStencilData." );
		return false;
	}

	return true;
}

static bool deserializeStencilComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCStencilData* val = (GCStencilData*)data;

	if( !cmp_read_bool( cmp, &( val->isStencil ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize isStencil of GCStencilData." );
		return false;
	}

	if( !cmp_read_int( cmp, &( val->stencilID ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize stencilID of GCStencilData." );
		return false;
	}

	return true;
}

//====
static bool serializeVec2TweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCVec2TweenData* val = (GCVec2TweenData*)data;

	if( !cmp_write_float( cmp, val->duration ) ) {
		llog( LOG_ERROR, "Unable to serialize duration of GCVec2TweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->timePassed ) ) {
		llog( LOG_ERROR, "Unable to serialize timePassed of GCVec2TweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->preDelay ) ) {
		llog( LOG_ERROR, "Unable to serialize preDelay of GCVec2TweenData." );
		return false;
	}
	
	if( !cmp_write_float( cmp, val->postDelay ) ) {
		llog( LOG_ERROR, "Unable to serialize postDelay of GCVec2TweenData." );
		return false;
	}

	if( !vec2_Serialize( cmp, &(val->startState) ) ) {
		llog( LOG_ERROR, "Unable to serialize startState of GCVec2TweenData." );
		return false;
	}

	if( !vec2_Serialize( cmp, &( val->endState ) ) ) {
		llog( LOG_ERROR, "Unable to serialize endState of GCVec2TweenData." );
		return false;
	}

	const char* easingID = ecps_GetTrackedIDFromTweenFunc( val->easing );
	uint32_t easingIDLen = (uint32_t)strlenNullTest( easingID );
	if( !cmp_write_str( cmp, easingID, easingIDLen ) ) {
		llog( LOG_ERROR, "Unable to serialize easing of GCVec2TweenData." );
		return false;
	}

	return true;
}

static bool deserializeVec2TweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCVec2TweenData* val = (GCVec2TweenData*)data;

	if( !cmp_read_float( cmp, &( val->duration ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize duration of GCVec2TweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->timePassed ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize timePassed of GCVec2TweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->preDelay ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize preDelay of GCVec2TweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->postDelay ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize postDelay of GCVec2TweenData." );
		return false;
	}

	if( !vec2_Deserialize( cmp, &( val->startState ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize startState of GCVec2TweenData." );
		return false;
	}

	if( !vec2_Deserialize( cmp, &( val->endState ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize endState of GCVec2TweenData." );
		return false;
	}

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );

	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for easing of GCVec2TweenData." );
	} else {
		val->easing = ecps_GetTrackedTweenFuncFromID( idBuffer );
	}

	return true;
}

//====
static bool serializeFloatTweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatTweenData* val = (GCFloatTweenData*)data;

	if( !cmp_write_float( cmp, val->duration ) ) {
		llog( LOG_ERROR, "Unable to serialize duration of GCFloatTweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->timePassed ) ) {
		llog( LOG_ERROR, "Unable to serialize timePassed of GCFloatTweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->preDelay ) ) {
		llog( LOG_ERROR, "Unable to serialize preDelay of GCFloatTweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->postDelay ) ) {
		llog( LOG_ERROR, "Unable to serialize postDelay of GCFloatTweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->startState ) ) {
		llog( LOG_ERROR, "Unable to serialize startState of GCFloatTweenData." );
		return false;
	}

	if( !cmp_write_float( cmp, val->endState ) ) {
		llog( LOG_ERROR, "Unable to serialize endState of GCFloatTweenData." );
		return false;
	}

	const char* easingID = ecps_GetTrackedIDFromTweenFunc( val->easing );
	uint32_t easingIDLen = (uint32_t)strlenNullTest( easingID );
	if( !cmp_write_str( cmp, easingID, easingIDLen ) ) {
		llog( LOG_ERROR, "Unable to serialize easing of GCFloatTweenData." );
		return false;
	}

	return true;
}

static bool deserializeFloatTweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatTweenData* val = (GCFloatTweenData*)data;

	if( !cmp_read_float( cmp, &( val->duration ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize duration of GCFloatTweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->timePassed ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize timePassed of GCFloatTweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->preDelay ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize preDelay of GCFloatTweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->postDelay ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize postDelay of GCFloatTweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->startState ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize startState of GCFloatTweenData." );
		return false;
	}

	if( !cmp_read_float( cmp, &( val->endState ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize endState of GCFloatTweenData." );
		return false;
	}

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );

	if( !cmp_read_str( cmp, idBuffer, &bufferSize ) ) {
		llog( LOG_ERROR, "Unable to deserialize id for easing of GCFloatTweenData." );
	} else {
		val->easing = ecps_GetTrackedTweenFuncFromID( idBuffer );
	}

	return true;
}

//====
static bool serializeGroupIDComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCGroupIDData* val = (GCGroupIDData*)data;

	if( !cmp_write_u32( cmp, val->groupID ) ) {
		llog( LOG_ERROR, "Unable to serialize groupID of GCGroupIDData." );
		return false;
	}

	return true;
}

static bool deserializeGroupdIDComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCGroupIDData* val = (GCGroupIDData*)data;

	if( !cmp_read_u32( cmp, &( val->groupID ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize groupID of GCGroupIDData." );
		return false;
	}

	return true;
}

//====
static bool serializeMountedPosComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCMountedPosOffset* val = (GCMountedPosOffset*)data;

	uint32_t localID = ecps_GetLocalIDFromSerializeEntityID( sbEntityInfo, val->parentID );
	if( !cmp_write_u32( cmp, localID ) ) {
		llog( LOG_ERROR, "Unable to serialize parentID of GCMountedPosOffset." );
		return false;
	}

	if( !vec2_Serialize( cmp, &( val->offset ) ) ) {
		llog( LOG_ERROR, "Unable to serialize offset of GCMountedPosOffset." );
		return false;
	}

	return true;
}

static bool deserializeMountedPosComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCMountedPosOffset* val = (GCMountedPosOffset*)data;

	uint32_t localID;
	if( !cmp_read_u32( cmp, &localID ) ) {
		llog( LOG_ERROR, "Unable to deserialize parentID of GCMountedPosOffset." );
		return false;
	}
	val->parentID = ecps_GetEntityIDfromSerializedLocalID( sbEntityInfo, localID );

	if( !vec2_Deserialize( cmp, &( val->offset ) ) ) {
		llog( LOG_ERROR, "Unable to deserialize offset of GCMountedPosOffset." );
		return false;
	}

	return true;
}

//=====================================================================================================

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

void gc_UnWatchEntity( ECPS* ecps, EntityID id )
{
	if( id == INVALID_ENTITY_ID ) return;
	if( !ecps_DoesEntityHaveComponentByID( ecps, id, gcWatchCompID ) ) return;
	ecps_RemoveComponentFromEntityByID( ecps, id, gcWatchCompID );
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
	gcPosCompID = ecps_AddComponentType( ecps, "GC_POS", 0, sizeof( GCPosData ), ALIGN_OF( GCPosData ), NULL, NULL, serializePosComp, deserializePosComp );
	gcClrCompID = ecps_AddComponentType( ecps, "GC_CLR", 0, sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL, serializeColorComp, deserializeColorComp );
	gcRotCompID = ecps_AddComponentType( ecps, "GC_ROT", 0, sizeof( GCRotData ), ALIGN_OF( GCRotData ), NULL, NULL, serializeRotComp, deserializeRotComp );
	gcScaleCompID = ecps_AddComponentType( ecps, "GC_SCL", 0, sizeof( GCScaleData ), ALIGN_OF( GCScaleData ), NULL, NULL, serializeScaleComp, deserializeScaleComp );
	gcSpriteCompID = ecps_AddComponentType( ecps, "GC_SPRT", 0, sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL, serializeSpriteComp, deserializeSpriteComp );
	gc3x3SpriteCompID = ecps_AddComponentType( ecps, "GC_3x3", 0, sizeof( GC3x3SpriteData ), ALIGN_OF( GC3x3SpriteData ), NULL, NULL, serialize3x3Comp, deserialize3x3Comp );
	gcColliderCompID = ecps_AddComponentType( ecps, "GC_COLL", 0, sizeof( GCColliderData ), ALIGN_OF( GCColliderData ), NULL, NULL, serializeCollComp, deserializeCollComp );
	gcPointerResponseCompID = ecps_AddComponentType( ecps, "GC_CLICK", 0, sizeof( GCPointerResponseData ), ALIGN_OF( GCPointerResponseData ), NULL, NULL, serializeClickResponseComp, deserializeClickResponseComp );
	gcCleanUpFlagCompID = ecps_AddComponentType( ecps, "GC_DEAD", 0, 0, 0, NULL, NULL, NULL, NULL );
	gcTextCompID = ecps_AddComponentType( ecps, "GC_TXT", 0, sizeof( GCTextData ), ALIGN_OF( GCTextData ), NULL, NULL, serializeTextComp, deserializeTextComp );
	gcFloatVal0CompID = ecps_AddComponentType( ecps, "GC_VAL0", 0, sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL, serializeVal0Comp, deserializeVal0Comp );
	gcWatchCompID = ecps_AddComponentType( ecps, "GC_WATCH", 0, 0, 0, NULL, NULL, NULL, NULL );
	gcMountedPosOffsetCompID = ecps_AddComponentType( ecps, "GC_MNTPOS", 0, sizeof( GCMountedPosOffset ), ALIGN_OF( GCMountedPosOffset ), NULL, NULL, serializeMountedPosComp, deserializeMountedPosComp );
	gcStencilCompID = ecps_AddComponentType( ecps, "GC_STENCIL", 0, sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL, serializeStencilComp, deserializeStencilComp );

	gcPosTweenCompID = ecps_AddComponentType( ecps, "P_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp, deserializeVec2TweenComp );
	gcScaleTweenCompID = ecps_AddComponentType( ecps, "S_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp, deserializeVec2TweenComp );
	gcAlphaTweenCompID = ecps_AddComponentType( ecps, "A_TWN", 0, sizeof( GCFloatTweenData ), ALIGN_OF( GCFloatTweenData ), NULL, NULL, serializeFloatTweenComp, serializeFloatTweenComp );

	gcGroupIDCompID = ecps_AddComponentType( ecps, "GRP", 0, sizeof( GCGroupIDData ), ALIGN_OF( GCGroupIDData ), NULL, NULL, serializeGroupIDComp, deserializeGroupdIDComp );
}
