#include "generalComponents.h"

#include <SDL3/SDL_assert.h>
#include <math.h>

#include "Utils/helpers.h"
#include "System/platformLog.h"
#include "System/ECPS/ecps_serialization.h"
#include "Math/mathUtil.h"
#include "System/memory.h"
#include "System/luaInterface.h"
#include "System/shared.h"

#include "Graphics/images.h"

ComponentID gcTransformCompID = INVALID_COMPONENT_ID;
ComponentID gcPosCompID = INVALID_COMPONENT_ID;
ComponentID gcClrCompID = INVALID_COMPONENT_ID;
ComponentID gcRotCompID = INVALID_COMPONENT_ID;
ComponentID gcScaleCompID = INVALID_COMPONENT_ID;
ComponentID gcSpriteCompID = INVALID_COMPONENT_ID;
ComponentID gcStencilCompID = INVALID_COMPONENT_ID;
ComponentID gc3x3SpriteCompID = INVALID_COMPONENT_ID;
ComponentID gcColliderCompID = INVALID_COMPONENT_ID;
ComponentID gcPointerCollisionCompID = INVALID_COMPONENT_ID;
ComponentID gcPointerResponseCompID = INVALID_COMPONENT_ID;
ComponentID gcCleanUpFlagCompID = INVALID_COMPONENT_ID;
ComponentID gcTextCompID = INVALID_COMPONENT_ID;
ComponentID gcFloatVal0CompID = INVALID_COMPONENT_ID;
ComponentID gcWatchCompID = INVALID_COMPONENT_ID;
ComponentID gcAlphaTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcPosTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcScaleTweenCompID = INVALID_COMPONENT_ID;
ComponentID gcGroupIDCompID = INVALID_COMPONENT_ID;
ComponentID gcSizeCompID = INVALID_COMPONENT_ID;
ComponentID gcShortLivedCompID = INVALID_COMPONENT_ID;
ComponentID gcAnimSpriteCompID = INVALID_COMPONENT_ID;

// serialization and deserialization functions
//====
static bool serializeGeneralCallback( cmp_ctx_t* cmp, SerializedEntityInfo* sbEntityInfo, GeneralCallback* callback, const char* type, const char* desc )
{
	SDL_assert( callback != NULL );

	CMP_WRITE( cmp, callback->type, cmp_write_int, type, desc, return false );
	if( callback->type == CBT_SOURCE ) {
		const char* functionID = ecps_GetTrackedIDFromECPSCallback( callback->source.callback );
		CMP_WRITE_STR( cmp, functionID, type, desc, return false );
	} else if( callback->type == CBT_LUA ) {
		CMP_WRITE_STR( cmp, callback->script.callback, type, desc, return false );
	} else {
		llog( LOG_ERROR, "Unknown callback type when saving %s for %s.", desc, type );
		return false;
	}

	return true;
}

static bool deserializeGeneralCallback( cmp_ctx_t* cmp, SerializedEntityInfo* sbEntityInfo, GeneralCallback* callback, const char* type, const char* desc )
{
	SDL_assert( callback != NULL );

	int readType;
	CMP_READ( cmp, readType, cmp_read_int, type, desc, return false );
	callback->type = (CallbackType)readType;

	if( callback->type == CBT_SOURCE ) {
		char idBuffer[128];
		uint32_t bufferSize = ARRAY_SIZE( idBuffer );
		CMP_READ_STR( cmp, idBuffer, bufferSize, type, desc, return false; );
		callback->source.callback = ecps_GetTrackedECPSCallbackFromID( idBuffer );
	} else if( callback->type == CBT_LUA ) {
		char idBuffer[128];
		uint32_t bufferSize = ARRAY_SIZE( idBuffer );
		CMP_READ_STR( cmp, idBuffer, bufferSize, "GCPointerResponseData", "overResponse", return false; );
		callback->script.callback = createStringCopy( idBuffer );
	} else {
		llog( LOG_ERROR, "Unknown callback type when loading %s for %s.", desc, type );
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

	CMP_WRITE( cmp, &( clr->currClr ), clr_Serialize, "GCColorData", "currClr", return false );
	CMP_WRITE( cmp, &( clr->futureClr ), clr_Serialize, "GCColorData", "futureClr", return false );

	return true;
}

static bool deserializeColorComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColorData* clr = (GCColorData*)data;

	CMP_READ( cmp, clr->currClr, clr_Deserialize, "GCColorData", "currClr", return false );
	CMP_READ( cmp, clr->futureClr, clr_Deserialize, "GCColorData", "futureClr", return false );

	return true;
}

//====
static bool serializeSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSpriteData* sprite = (GCSpriteData*)data;

	CMP_WRITE( cmp, sprite->camFlags, cmp_write_u32, "GCSpriteData", "camFlags", return false );
	CMP_WRITE( cmp, sprite->depth, cmp_write_s8, "GCSpriteData", "depth", return false );
	const char* imgID = img_GetImgStringID( sprite->img );
	CMP_WRITE_STR( cmp, imgID, "GCSpriteData", "img", return false );

	return true;
}

static bool deserializeSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSpriteData* sprite = (GCSpriteData*)data;

	CMP_READ( cmp, sprite->camFlags, cmp_read_u32, "GCSpriteData", "camFlags", return false );
	CMP_READ( cmp, sprite->depth, cmp_read_s8, "GCSpriteData", "depth", return false );

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );
	CMP_READ_STR( cmp, idBuffer, bufferSize, "GCSpriteData", "img", return false );
	sprite->img = img_GetExistingByID( idBuffer );
	if( sprite->img == -1 ) {
		llog( LOG_ERROR, "Unable to find img with id %s for GCSpriteData.", idBuffer );
	}

	return true;
}

//====
static bool serialize3x3Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GC3x3SpriteData* sprite = (GC3x3SpriteData*)data;

	CMP_WRITE( cmp, sprite->camFlags, cmp_write_u32, "GC3x3SpriteData", "camFlags", return false );
	CMP_WRITE( cmp, sprite->depth, cmp_write_s8, "GC3x3SpriteData", "depth", return false );
	CMP_WRITE( cmp, &sprite->size, vec2_Serialize, "GC3x3SpriteData", "size", return false );

	const char* imgID = img_GetImgStringID( sprite->img );
	CMP_WRITE_STR( cmp, imgID, "GCSpriteData", "img", return false );

	CMP_WRITE( cmp, sprite->topBorder, cmp_write_u32, "GC3x3SpriteData", "topBorder", return false );
	CMP_WRITE( cmp, sprite->bottomBorder, cmp_write_u32, "GC3x3SpriteData", "bottomBorder", return false );
	CMP_WRITE( cmp, sprite->leftBorder, cmp_write_u32, "GC3x3SpriteData", "leftBorder", return false );
	CMP_WRITE( cmp, sprite->rightBorder, cmp_write_u32, "GC3x3SpriteData", "rightBorder", return false );

	return true;
}

static bool deserialize3x3Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GC3x3SpriteData* sprite = (GC3x3SpriteData*)data;

	CMP_READ( cmp, sprite->camFlags, cmp_read_u32, "GC3x3SpriteData", "camFlags", return false );
	CMP_READ( cmp, sprite->depth, cmp_read_s8, "GC3x3SpriteData", "depth", return false );
	CMP_READ( cmp, sprite->size, vec2_Deserialize, "GC3x3SpriteData", "size", return false );

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );
	CMP_READ_STR( cmp, idBuffer, bufferSize, "GC3x3SpriteData", "img", return false );
	sprite->img = img_GetExistingByID( idBuffer );
	if( sprite->img == -1 ) {
		llog( LOG_ERROR, "Unable to find img with id %s for GC3x3SpriteData.", idBuffer );
	}

	CMP_READ( cmp, sprite->topBorder, cmp_read_u32, "GC3x3SpriteData", "topBorder", return false );
	CMP_READ( cmp, sprite->bottomBorder, cmp_read_u32, "GC3x3SpriteData", "bottomBorder", return false );
	CMP_READ( cmp, sprite->leftBorder, cmp_read_u32, "GC3x3SpriteData", "leftBorder", return false );
	CMP_READ( cmp, sprite->rightBorder, cmp_read_u32, "GC3x3SpriteData", "rightBorder", return false );

	return true;
}

//====
static bool serializeCollComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCColliderData* coll = (GCColliderData*)data;

	CMP_WRITE( cmp, coll->base.type, cmp_write_int, "GCColliderData", "base type", return false );

	switch( coll->base.type )
	{
	case CT_AABB:
		CMP_WRITE( cmp, &(coll->aab.halfDim), vec2_Serialize, "GCColliderData", "halfDim", return false; );
		break;
	case CT_CIRCLE:
		CMP_WRITE( cmp, coll->circle.radius, cmp_write_float, "GCColliderData", "radius", return false; );
		break;
	case CT_HALF_SPACE:
		CMP_WRITE( cmp, &( coll->halfSpace.normal ), vec2_Serialize, "GCColliderData", "normal", return false; );
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
	CMP_READ( cmp, readType, cmp_read_int, "GCColliderData", "base type", return false );
	coll->base.type = (enum ColliderType)readType;

	switch( coll->base.type )
	{
	case CT_AABB:
		CMP_READ( cmp, coll->aab.halfDim, vec2_Deserialize, "GCColliderData", "halfDim", return false; );
		break;
	case CT_CIRCLE:
		CMP_READ( cmp, coll->circle.radius, cmp_read_float, "GCColliderData", "radius", return false; );
		break;
	case CT_HALF_SPACE:
		CMP_READ( cmp, coll->halfSpace.normal, vec2_Deserialize, "GCColliderData", "normal", return false; );
		break;
	}

	return true;
}

//====
static bool serializeClickCollisionComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerCollisionData* click = (GCPointerCollisionData*)data;

	CMP_WRITE( cmp, &( click->collisionHalfDim ), vec2_Serialize, "GCPointerResponseData", "collisionHalfDim", return false );
	CMP_WRITE( cmp, click->camFlags, cmp_write_uint, "GCPointerResponseData", "camFlags", return false );
	CMP_WRITE( cmp, click->priority, cmp_write_s32, "GCPointerResponseData", "priority", return false );

	return true;
}

static bool deserializeClickCollisionComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerCollisionData* click = (GCPointerCollisionData*)data;

	CMP_READ( cmp, click->collisionHalfDim, vec2_Deserialize, "GCPointerResponseData", "collisionHalfDim", return false );
	CMP_READ( cmp, click->camFlags, cmp_read_uint, "GCPointerResponseData", "camFlags", return false );
	CMP_READ( cmp, click->priority, cmp_read_s32, "GCPointerResponseData", "priority", return false );

	return true;
}

//====
static bool serializeClickFuncResponseComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerResponseData* response = (GCPointerResponseData*)data;

	if( !serializeGeneralCallback( cmp, sbEntityInfo, &response->overResponse, "GCPointerResponseData", "overResponse" ) ) return false;
	if( !serializeGeneralCallback( cmp, sbEntityInfo, &response->leaveResponse, "GCPointerResponseData", "leaveResponse" ) ) return false;
	if( !serializeGeneralCallback( cmp, sbEntityInfo, &response->pressResponse, "GCPointerResponseData", "pressResponse" ) ) return false;
	if( !serializeGeneralCallback( cmp, sbEntityInfo, &response->releaseResponse, "GCPointerResponseData", "releaseResponse" ) ) return false;

	return true;
}

static bool deserializeClickFuncResponseComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCPointerResponseData* response = (GCPointerResponseData*)data;

	if( !deserializeGeneralCallback( cmp, sbEntityInfo, &response->overResponse, "GCPointerResponseData", "overResponse" ) ) return false;
	if( !deserializeGeneralCallback( cmp, sbEntityInfo, &response->leaveResponse, "GCPointerResponseData", "leaveResponse" ) ) return false;
	if( !deserializeGeneralCallback( cmp, sbEntityInfo, &response->pressResponse, "GCPointerResponseData", "pressResponse" ) ) return false;
	if( !deserializeGeneralCallback( cmp, sbEntityInfo, &response->releaseResponse, "GCPointerResponseData", "releaseResponse" ) ) return false;

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

	CMP_WRITE( cmp, val->currValue, cmp_write_float, "GCFloatVal0Data", "currValue", return false );
	CMP_WRITE( cmp, val->futureValue, cmp_write_float, "GCFloatVal0Data", "futureValue", return false );

	return true;
}

static bool deserializeVal0Comp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatVal0Data* val = (GCFloatVal0Data*)data;

	CMP_READ( cmp, val->currValue, cmp_read_float, "GCFloatVal0Data", "currValue", return false );
	CMP_READ( cmp, val->futureValue, cmp_read_float, "GCFloatVal0Data", "futureValue", return false );

	return true;
}

//====
static bool serializeStencilComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCStencilData* val = (GCStencilData*)data;

	CMP_WRITE( cmp, val->isStencil, cmp_write_bool, "GCStencilData", "isStencil", return false );
	CMP_WRITE( cmp, val->stencilID, cmp_write_int, "GCStencilData", "stencilID", return false );

	return true;
}

static bool deserializeStencilComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCStencilData* val = (GCStencilData*)data;

	CMP_READ( cmp, val->isStencil, cmp_read_bool, "GCStencilData", "isStencil", return false );
	CMP_READ( cmp, val->stencilID, cmp_read_int, "GCStencilData", "stencilID", return false );

	return true;
}

//====
static bool serializeVec2TweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCVec2TweenData* val = (GCVec2TweenData*)data;

	CMP_WRITE( cmp, val->duration, cmp_write_float, "GCVec2TweenData", "duration", return false );
	CMP_WRITE( cmp, val->timePassed, cmp_write_float, "GCVec2TweenData", "timePassed", return false );
	CMP_WRITE( cmp, val->preDelay, cmp_write_float, "GCVec2TweenData", "preDelay", return false );
	CMP_WRITE( cmp, val->postDelay, cmp_write_float, "GCVec2TweenData", "postDelay", return false );
	CMP_WRITE( cmp, &(val->startState), vec2_Serialize, "GCVec2TweenData", "startState", return false );
	CMP_WRITE( cmp, &(val->endState), vec2_Serialize, "GCVec2TweenData", "endState", return false );
	
	const char* easingID = ecps_GetTrackedIDFromTweenFunc( val->easing );
	CMP_WRITE_STR( cmp, easingID, "GCVec2TweenData", "easing", return false );

	return true;
}

static bool deserializeVec2TweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCVec2TweenData* val = (GCVec2TweenData*)data;

	CMP_READ( cmp, val->duration, cmp_read_float, "GCVec2TweenData", "duration", return false );
	CMP_READ( cmp, val->timePassed, cmp_read_float, "GCVec2TweenData", "timePassed", return false );
	CMP_READ( cmp, val->preDelay, cmp_read_float, "GCVec2TweenData", "preDelay", return false );
	CMP_READ( cmp, val->postDelay, cmp_read_float, "GCVec2TweenData", "postDelay", return false );
	CMP_READ( cmp, val->startState, vec2_Deserialize, "GCVec2TweenData", "startState", return false );
	CMP_READ( cmp, val->endState, vec2_Deserialize, "GCVec2TweenData", "endState", return false );

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );
	CMP_READ_STR( cmp, idBuffer, bufferSize, "GCVec2TweenData", "easing", return false );
	val->easing = ecps_GetTrackedTweenFuncFromID( idBuffer );

	return true;
}

//====
static bool serializeFloatTweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatTweenData* val = (GCFloatTweenData*)data;


	CMP_WRITE( cmp, val->duration, cmp_write_float, "GCFloatTweenData", "duration", return false );
	CMP_WRITE( cmp, val->timePassed, cmp_write_float, "GCFloatTweenData", "timePassed", return false );
	CMP_WRITE( cmp, val->preDelay, cmp_write_float, "GCFloatTweenData", "preDelay", return false );
	CMP_WRITE( cmp, val->postDelay, cmp_write_float, "GCFloatTweenData", "postDelay", return false );
	CMP_WRITE( cmp, val->startState, cmp_write_float, "GCFloatTweenData", "startState", return false );
	CMP_WRITE( cmp, val->endState, cmp_write_float, "GCFloatTweenData", "endState", return false );

	const char* easingID = ecps_GetTrackedIDFromTweenFunc( val->easing );
	CMP_WRITE_STR( cmp, easingID, "GCFloatTweenData", "easing", return false );

	return true;
}

static bool deserializeFloatTweenComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCFloatTweenData* val = (GCFloatTweenData*)data;

	CMP_READ( cmp, val->duration, cmp_read_float, "GCFloatTweenData", "duration", return false );
	CMP_READ( cmp, val->timePassed, cmp_read_float, "GCFloatTweenData", "timePassed", return false );
	CMP_READ( cmp, val->preDelay, cmp_read_float, "GCFloatTweenData", "preDelay", return false );
	CMP_READ( cmp, val->postDelay, cmp_read_float, "GCFloatTweenData", "postDelay", return false );
	CMP_READ( cmp, val->startState, cmp_read_float, "GCFloatTweenData", "startState", return false );
	CMP_READ( cmp, val->endState, cmp_read_float, "GCFloatTweenData", "endState", return false );

	char idBuffer[128];
	uint32_t bufferSize = ARRAY_SIZE( idBuffer );
	CMP_READ_STR( cmp, idBuffer, bufferSize, "GCFloatTweenData", "easing", return false );
	val->easing = ecps_GetTrackedTweenFuncFromID( idBuffer );

	return true;
}

//====
static bool serializeGroupIDComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCGroupIDData* val = (GCGroupIDData*)data;

	CMP_WRITE( cmp, val->groupID, cmp_write_u32, "GCGroupIDData", "groupID", return false );

	return true;
}

static bool deserializeGroupdIDComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCGroupIDData* val = (GCGroupIDData*)data;

	CMP_READ( cmp, val->groupID, cmp_read_u32, "GCGroupIDData", "groupID", return false );

	return true;
}

//====
static bool serializeTranformComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCTransformData* val = (GCTransformData*)data;

	CMP_WRITE( cmp, &( val->currState.pos ), vec2_Serialize, "GCTransformData", "currState pos", return false );
	CMP_WRITE( cmp, &( val->currState.scale ), vec2_Serialize, "GCTransformData", "currState scale", return false );
	CMP_WRITE( cmp, val->currState.rotRad, cmp_write_float, "GCTransformData", "currState rotRad", return false );

	CMP_WRITE( cmp, &( val->futureState.pos ), vec2_Serialize, "GCTransformData", "futureState pos", return false );
	CMP_WRITE( cmp, &( val->futureState.scale ), vec2_Serialize, "GCTransformData", "futureState scale", return false );
	CMP_WRITE( cmp, val->futureState.rotRad, cmp_write_float, "GCTransformData", "futureState rotRad", return false );

	uint32_t localParentID = ecps_GetLocalIDFromSerializeEntityID( sbEntityInfo, val->parentID );
	CMP_WRITE( cmp, localParentID, cmp_write_u32, "GCTransformData", "parentID", return false );

	uint32_t localFirstChildID = ecps_GetLocalIDFromSerializeEntityID( sbEntityInfo, val->firstChildID );
	CMP_WRITE( cmp, localFirstChildID, cmp_write_u32, "GCTransformData", "firstChildID", return false );

	uint32_t localNextSiblingID = ecps_GetLocalIDFromSerializeEntityID( sbEntityInfo, val->nextSiblingID );
	CMP_WRITE( cmp, localNextSiblingID, cmp_write_u32, "GCTransformData", "nextSiblingID", return false );

	return true;
}

static bool deserializeTranformComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCTransformData* val = (GCTransformData*)data;

	CMP_READ( cmp, val->currState.pos, vec2_Deserialize, "GCTransformData", "currState pos", return false );
	CMP_READ( cmp, val->currState.scale, vec2_Deserialize, "GCTransformData", "currState scale", return false );
	CMP_READ( cmp, val->currState.rotRad, cmp_read_float, "GCTransformData", "currState rotRad", return false );

	CMP_READ( cmp, val->futureState.pos, vec2_Deserialize, "GCTransformData", "futureState pos", return false );
	CMP_READ( cmp, val->futureState.scale, vec2_Deserialize, "GCTransformData", "futureState scale", return false );
	CMP_READ( cmp, val->futureState.rotRad, cmp_read_float, "GCTransformData", "futureState rotRad", return false );

	uint32_t localParentID;
	CMP_READ( cmp, localParentID, cmp_read_u32, "GCTransformData", "parentID", return false );
	val->parentID = ecps_GetEntityIDfromSerializedLocalID( sbEntityInfo, localParentID );

	uint32_t localFirstChildID;
	CMP_READ( cmp, localFirstChildID, cmp_read_u32, "GCTransformData", "firstChildID", return false );
	val->firstChildID = ecps_GetEntityIDfromSerializedLocalID( sbEntityInfo, localFirstChildID );

	uint32_t localNextSiblingID;
	CMP_READ( cmp, localNextSiblingID, cmp_read_u32, "GCTransformData", "nextSiblingID", return false );
	val->nextSiblingID = ecps_GetEntityIDfromSerializedLocalID( sbEntityInfo, localNextSiblingID );

	return true;
}

//=====================================================================================================

static bool serializeSizeComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSizeData* val = (GCSizeData*)data;

	CMP_WRITE( cmp, &( val->currentSize ), vec2_Serialize, "GCSizeData", "currentSize", return false );
	CMP_WRITE( cmp, &( val->futureSize ), vec2_Serialize, "GCSizeData", "futureSize", return false );

	return true;
}

static bool deserializeSizeComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	GCSizeData* val = (GCSizeData*)data;

	CMP_READ( cmp, val->currentSize, vec2_Deserialize, "GCSizeData", "currentSize", return false );
	CMP_READ( cmp, val->futureSize, vec2_Deserialize, "GCSizeData", "futureSize", return false );

	return true;
}

//====
static bool serializeShortLivedComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	SDL_assert_always( false && "serializeShortLivedComp: Not implemented yet." );

	return true;
}

static bool deserializeShortLivedComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	SDL_assert_always( false && "deserializeShortLivedComp: Not implemented yet." );

	return true;
}

//====
static bool serializeAnimatedSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	SDL_assert_always( false && "serializeAnimatedSpriteComp: Not implemented yet." );

	return true;
}

static bool deserializeAnimatedSpriteComp( cmp_ctx_t* cmp, void* data, SerializedEntityInfo* sbEntityInfo )
{
	SDL_assert( cmp != NULL );
	SDL_assert( data != NULL );

	SDL_assert_always( false && "deserializeAnimatedSpriteComp: Not implemented yet." );

	return true;
}

//=====================================================================================================
void gc_GetLerpedGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, float t, Matrix3* outMat )
{
	SDL_assert( tf != NULL );
	SDL_assert( outMat != NULL );
	SDL_assert( t >= 0.0f && t <= 1.0f );

	if( tf->lastMatVal == t ) {
		( *outMat ) = tf->mat;
	}

	// get the global matrix of the parent transform
	Matrix3 parentMat;
	if( tf->parentID == INVALID_ENTITY_ID ) {
		parentMat = IDENTITY_MATRIX_3;
	} else {
		// generally this shouldn't go too deep
		GCTransformData* parentTf = NULL;
		if( ecps_GetComponentFromEntityByID( ecps, tf->parentID, gcTransformCompID, &parentTf ) ) {
			gc_GetLerpedGlobalMatrix( ecps, parentTf, &VEC2_ZERO, t, &parentMat );
		}
	}

	Vector2 pos;
	Vector2 scale;
	float rotRad;

	vec2_Lerp( &( tf->currState.pos ), &( tf->futureState.pos ), t, &pos );
	vec2_Lerp( &( tf->currState.scale ), &( tf->futureState.scale ), t, &scale );
	rotRad = radianRotLerp( tf->currState.rotRad, tf->futureState.rotRad, t );

	mat3_CreateRenderTransform( &pos, rotRad, imageOffset, &scale, &( tf->mat ) );
	tf->lastMatVal = t;

	mat3_Multiply( &parentMat, &( tf->mat ), outMat );
}

void gc_GetCurrGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, Matrix3* outMat )
{
	gc_GetLerpedGlobalMatrix( ecps, tf, imageOffset, 0.0f, outMat );
}

void gc_GetFutureGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, Matrix3* outMat )
{
	gc_GetLerpedGlobalMatrix( ecps, tf, imageOffset, 1.0f, outMat );
}

static void detachInternal( ECPS* ecps, EntityID id, GCTransformData* tf, bool ignoreTreeOperations )
{
	if( tf->parentID == INVALID_ENTITY_ID ) {
		return;
	}

	Matrix3 globalCurrMat;
	Matrix3 globalFutureMat;
	gc_GetCurrGlobalMatrix( ecps, tf, &VEC2_ZERO, &globalCurrMat );
	gc_GetFutureGlobalMatrix( ecps, tf, &VEC2_ZERO, &globalFutureMat );

	// we'll ignore the tree operations if we're doing a large batch of removals when destroying an entity or removing the component, whatever is doing the removal will deal with that
	if( !ignoreTreeOperations ) {
		// get the parent
		GCTransformData* parentTf = NULL;
		ecps_GetComponentFromEntityByID( ecps, tf->parentID, gcTransformCompID, &parentTf );
		ASSERT_AND_IF_NOT( parentTf != NULL )
		{
			return;
		}

		if( parentTf->firstChildID != id ) {
			// we're not the first child, find the previous sibling and swap out their next with ours
			EntityID nextChild = parentTf->firstChildID;
			GCTransformData* currChildTf = NULL;
			do {
				ecps_GetComponentFromEntityByID( ecps, nextChild, gcTransformCompID, &currChildTf );
				ASSERT_AND_IF_NOT( currChildTf != NULL )
				{
					return;
				}
				nextChild = currChildTf->nextSiblingID;
			} while( nextChild != id );
			currChildTf->nextSiblingID = tf->nextSiblingID;
		} else {
			// we are the first child, pop ourself off the front of the list
			parentTf->firstChildID = tf->nextSiblingID;
		}

		tf->nextSiblingID = INVALID_ENTITY_ID;
		tf->parentID = INVALID_ENTITY_ID;
	}

	mat3_TransformDecompose( &globalCurrMat, &( tf->currState.pos ), &( tf->currState.rotRad ), &( tf->currState.scale ) );
	mat3_TransformDecompose( &globalFutureMat, &( tf->futureState.pos ), &( tf->futureState.rotRad ), &( tf->futureState.scale ) );
}

// return if testID is a descendant of any of the 
static bool isDescendantOf( ECPS* ecps, EntityID testID, GCTransformData* tf )
{
	if( tf == NULL ) return false;

	while( tf->parentID != INVALID_ENTITY_ID ) {
		if( tf->parentID == testID ) return true;

		ecps_GetComponentFromEntityByID( ecps, tf->parentID, gcTransformCompID, &tf );
		ASSERT_AND_IF_NOT( tf != NULL ) {
			return false;
		}
	}

	return false;
}

// // retrieves the local transform stuff
Vector2 gc_GetLocalPos( GCTransformData* tf )
{
	SDL_assert( tf != NULL );
	return tf->futureState.pos;
}

float gc_GetLocalRotRad( GCTransformData* tf )
{
	SDL_assert( tf != NULL );
	return tf->futureState.rotRad;
}

Vector2 gc_GetLocalScale( GCTransformData* tf )
{
	SDL_assert( tf != NULL );
	return tf->futureState.scale;
}

// sets the local transform stuff
void gc_SetTransformLocalPos( GCTransformData* tf, Vector2 pos)
{
	SDL_assert( tf != NULL );
	tf->futureState.pos = pos;
	tf->lastMatVal = INFINITY;
}

void gc_SetTransformLocalRot( GCTransformData* tf, float rotRad )
{
	SDL_assert( tf != NULL );
	tf->futureState.rotRad = rotRad;
	tf->lastMatVal = INFINITY;
}

void gc_SetTransformLocalVectorScale( GCTransformData* tf, Vector2 scale )
{
	SDL_assert( tf != NULL );
	tf->futureState.scale = scale;
	tf->lastMatVal = INFINITY;
}

void gc_SetTransformLocalFloatScale( GCTransformData* tf, float scale )
{
	SDL_assert( tf != NULL );
	tf->futureState.scale = vec2( scale, scale );
	tf->lastMatVal = INFINITY;
}

// adds an offset to the local transform stuff
void gc_AdjustLocalPos( GCTransformData* tf, Vector2* adj)
{
	SDL_assert( tf != NULL );
	SDL_assert( adj != NULL );
	vec2_Add( &( tf->futureState.pos ), adj, &( tf->futureState.pos ) );
	tf->lastMatVal = INFINITY;
}

void gc_AdjustLocalRot( GCTransformData* tf, float adjRad)
{
	SDL_assert( tf != NULL );
	tf->futureState.rotRad = radianRotWrap( tf->futureState.rotRad + adjRad );
	tf->lastMatVal = INFINITY;
}

void gc_AdjustLocalVectorScale( GCTransformData* tf, Vector2* adj )
{
	SDL_assert( tf != NULL );
	SDL_assert( adj != NULL );
	vec2_Add( &( tf->futureState.scale ), adj, &( tf->futureState.scale ) );
	tf->lastMatVal = INFINITY;
}

void gc_AdjustLocalFloatScale( GCTransformData* tf, float adj )
{
	SDL_assert( tf != NULL );
	tf->futureState.scale.x += adj;
	tf->futureState.scale.y += adj;
	tf->lastMatVal = INFINITY;
}

// changes everything, making sure there is no lerping during the rendering
void gc_TeleportToLocalPos( GCTransformData* tf, Vector2 pos )
{
	SDL_assert( tf != NULL );
	tf->currState.pos = tf->futureState.pos = pos;
	tf->lastMatVal = INFINITY;
}

void gc_TeleportToLocalRot( GCTransformData* tf, float rotRad )
{
	SDL_assert( tf != NULL );
	tf->currState.rotRad = tf->futureState.rotRad = rotRad;
	tf->lastMatVal = INFINITY;
}

void gc_TeleportToLocalVectorScale( GCTransformData* tf, Vector2 scale )
{
	SDL_assert( tf != NULL );
	tf->currState.scale = tf->futureState.scale = scale;
	tf->lastMatVal = INFINITY;
}

void gc_TeleportToLocalFloatScale( GCTransformData* tf, float scale )
{
	SDL_assert( tf != NULL );
	tf->currState.scale = tf->futureState.scale = vec2( scale, scale );
	tf->lastMatVal = INFINITY;
}

void gc_FinalizeTransform( GCTransformData* tf )
{
	tf->currState = tf->futureState;
	tf->lastMatVal = INFINITY;
}

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

	GCTransformData* parentTf = NULL;
	GCTransformData* childTf = NULL;

	if( !ecps_GetComponentFromEntity( &parent, gcTransformCompID, &parentTf ) ) {
		llog( LOG_DEBUG, "Parent has no transform component while attempting to mount." );
		return;
	}

	if( !ecps_GetComponentFromEntity( &child, gcTransformCompID, &childTf ) ) {
		llog( LOG_DEBUG, "Child has no transform component while attempting to mount." );
		return;
	}

	// enforce the treeness, make sure we won't create any cycles
	//  since we'll detach the node we want to make a child the only case where this
	//  should happen would be when we would try to make the root node of a tree the
	//  child of one of it's descendants
	bool isDescendant = isDescendantOf( ecps, childID, parentTf );
	ASSERT_AND_IF_NOT( !isDescendant ) {
		return;
	}

	// see if we can get the inverse of the parents global matrix, if we can't for some reason then let use know
	// TODO: this might cause some snapping with the initial attachment, need to test this
	Matrix3 mat;
	gc_GetFutureGlobalMatrix( ecps, parentTf, &VEC2_ZERO, &mat );
	Matrix3 inverseMat;
	bool success = mat3_Inverse( &mat, &inverseMat );
	ASSERT_AND_IF_NOT( success ) {
		return;
	}

	// detach first
	detachInternal( ecps, childID, childTf, false );

	// set up the references, keep the previous order of attached entities
	if( parentTf->firstChildID == INVALID_ENTITY_ID ) {
		// no other children, so set as first
		parentTf->firstChildID = childID;
	} else {
		// go through the children till we find the last one
		// TODO: do we want to store the last child as well as the first child to make this faster?
		EntityID nextChild = parentTf->firstChildID;
		GCTransformData* currChildTf = NULL;
		do {
			ecps_GetComponentFromEntityByID( ecps, nextChild, gcTransformCompID, &currChildTf );
			ASSERT_AND_IF_NOT( currChildTf != NULL ) {
				return;
			}
			nextChild = currChildTf->nextSiblingID;
		} while( nextChild != INVALID_ENTITY_ID );
		currChildTf->nextSiblingID = childID;
	}

	childTf->parentID = parentID;

	// calculate the new local position, rotation, and scale. the detach above should have reset everything to global values
	Matrix3 newFutureGlobal, newCurrGlobal, newFutureLocal, newCurrLocal;
	mat3_CreateTransform( &( childTf->futureState.pos ), childTf->futureState.rotRad, &( childTf->futureState.scale ), &newFutureGlobal );
	mat3_CreateTransform( &( childTf->currState.pos ), childTf->currState.rotRad, &( childTf->currState.scale ), &newCurrGlobal );

	mat3_Multiply( &inverseMat, &newFutureGlobal, &newFutureLocal );
	mat3_Multiply( &inverseMat, &newCurrGlobal, &newCurrLocal );

	mat3_TransformDecompose( &newFutureLocal, &( childTf->futureState.pos ), &( childTf->futureState.rotRad ), &( childTf->futureState.scale ) );
	mat3_TransformDecompose( &newCurrLocal, &( childTf->currState.pos ), &( childTf->currState.rotRad ), &( childTf->currState.scale ) );
}

void gc_UnmountEntityFromParent( ECPS* ecps, EntityID id )
{
	if( id == INVALID_ENTITY_ID ) return;

	GCTransformData* tf = NULL;

	if( !ecps_GetComponentFromEntityByID( ecps, id, gcTransformCompID, &tf ) ) {
		llog( LOG_DEBUG, "Unable to find component when unmounting." );
	}

	detachInternal( ecps, id, tf, false );
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

void gc_ColliderDataToCollider( GCColliderData* colliderData, GCTransformData* tfData, Collider* outCollider )
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
		outCollider->aabb.center = tfData->futureState.pos;
		break;
	case CT_CIRCLE:
		outCollider->type = CT_CIRCLE;
		outCollider->circle.radius = colliderData->circle.radius;
		outCollider->aabb.center = tfData->futureState.pos;
		break;
	case CT_HALF_SPACE:
		outCollider->type = CT_HALF_SPACE;
		collision_CalculateHalfSpace( &(tfData->futureState.pos), &(colliderData->halfSpace.normal), outCollider );
		break;
	default:
		SDL_assert( false && "Invalid collider type." );
	}
}

// if an entity with a GCTransformComp is destoyed or we remove the component clean up the attachments.
static void transformCleanUp( ECPS* ecps, const Entity* entity, void* compData, bool fullCleanUp )
{
	// if it's a full clean up we don't need to worry about doing this
	if( fullCleanUp ) return;

	GCTransformData* tfData = (GCTransformData*)compData;

	// detach ourself from our parent, let it handle dealing with the tree adjustments
	detachInternal( ecps, entity->id, tfData, false );

	// detach all our children from us
	EntityID child = tfData->firstChildID;
	while( child != INVALID_ENTITY_ID ) {
		GCTransformData* childTfData = NULL;
		ecps_GetComponentFromEntityByID( ecps, child, gcTransformCompID, &childTfData );
		ASSERT_AND_IF_NOT( childTfData != NULL ) {
			// found a mounted entity that does not have a transform component attached to it
			child = INVALID_ENTITY_ID;
		} else {
			// handle the transition from local to global
			detachInternal( ecps, child, childTfData, true );

			// tree adjustments
			childTfData->parentID = INVALID_ENTITY_ID;
			child = childTfData->nextSiblingID;
			childTfData->nextSiblingID = INVALID_ENTITY_ID;
		}
	}
}

static void textCleanUp( ECPS* ecps, const Entity* entity, void* compData, bool fullCleanUp )
{
	GCTextData* data = (GCTextData*)compData;

	if( data->textIsDynamic ) {
		mem_Release( data->text );
		data->text = NULL;
	}
}

static void pointerScriptCleanUp( ECPS* ecps, const Entity* entity, void* compData, bool fullCleanUp )
{
	GCPointerResponseData* data = (GCPointerResponseData*)compData;

	if( data->overResponse.type == CBT_LUA ) mem_Release( data->overResponse.script.callback );
	if( data->leaveResponse.type == CBT_LUA ) mem_Release( data->leaveResponse.script.callback );
	if( data->pressResponse.type == CBT_LUA ) mem_Release( data->pressResponse.script.callback );
	if( data->releaseResponse.type == CBT_LUA ) mem_Release( data->releaseResponse.script.callback );
}

void gc_Register( ECPS* ecps )
{
	gcTransformCompID = ecps_AddComponentType( ecps, "GC_TF", 0, sizeof( GCTransformData ), ALIGN_OF( GCTransformData ), transformCleanUp, NULL, serializeTranformComp, deserializeTranformComp );
	gcClrCompID = ecps_AddComponentType( ecps, "GC_CLR", 0, sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL, serializeColorComp, deserializeColorComp );
	gcSpriteCompID = ecps_AddComponentType( ecps, "GC_SPRT", 0, sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL, serializeSpriteComp, deserializeSpriteComp );
	gc3x3SpriteCompID = ecps_AddComponentType( ecps, "GC_3x3", 0, sizeof( GC3x3SpriteData ), ALIGN_OF( GC3x3SpriteData ), NULL, NULL, serialize3x3Comp, deserialize3x3Comp );
	gcColliderCompID = ecps_AddComponentType( ecps, "GC_COLL", 0, sizeof( GCColliderData ), ALIGN_OF( GCColliderData ), NULL, NULL, serializeCollComp, deserializeCollComp );
	gcPointerCollisionCompID = ecps_AddComponentType( ecps, "GC_CLICK", 0, sizeof( GCPointerCollisionData ), ALIGN_OF( GCPointerCollisionData ), NULL, NULL, serializeClickCollisionComp, deserializeClickCollisionComp );
	gcPointerResponseCompID = ecps_AddComponentType( ecps, "GC_CLK", 0, sizeof( GCPointerResponseData ), ALIGN_OF( GCPointerResponseData ), pointerScriptCleanUp, NULL, serializeClickFuncResponseComp, deserializeClickFuncResponseComp );
	gcCleanUpFlagCompID = ecps_AddComponentType( ecps, "GC_DEAD", 0, 0, 0, NULL, NULL, NULL, NULL );
	gcTextCompID = ecps_AddComponentType( ecps, "GC_TXT", 0, sizeof( GCTextData ), ALIGN_OF( GCTextData ), textCleanUp, NULL, serializeTextComp, deserializeTextComp );
	gcFloatVal0CompID = ecps_AddComponentType( ecps, "GC_VAL0", 0, sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL, serializeVal0Comp, deserializeVal0Comp );
	gcWatchCompID = ecps_AddComponentType( ecps, "GC_WATCH", 0, 0, 0, NULL, NULL, NULL, NULL );
	gcStencilCompID = ecps_AddComponentType( ecps, "GC_STENCIL", 0, sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL, serializeStencilComp, deserializeStencilComp );
	gcSizeCompID = ecps_AddComponentType( ecps, "GC_SIZE", 0, sizeof( GCSizeData ), ALIGN_OF( GCSizeData ), NULL, NULL, serializeSizeComp, deserializeSizeComp );

	gcPosTweenCompID = ecps_AddComponentType( ecps, "P_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp, deserializeVec2TweenComp );
	gcScaleTweenCompID = ecps_AddComponentType( ecps, "S_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp, deserializeVec2TweenComp );
	gcAlphaTweenCompID = ecps_AddComponentType( ecps, "A_TWN", 0, sizeof( GCFloatTweenData ), ALIGN_OF( GCFloatTweenData ), NULL, NULL, serializeFloatTweenComp, deserializeFloatTweenComp );
	gcShortLivedCompID = ecps_AddComponentType( ecps, "GC_LIFE", 0, sizeof( GCShortLivedData ), ALIGN_OF( GCShortLivedData ), NULL, NULL, serializeShortLivedComp, deserializeShortLivedComp );
	gcAnimSpriteCompID = ecps_AddComponentType( ecps, "ANIM_SPR", 0, sizeof( GCAnimatedSpriteData ), ALIGN_OF( GCAnimatedSpriteData ), NULL, NULL, serializeAnimatedSpriteComp, deserializeAnimatedSpriteComp );

	gcGroupIDCompID = ecps_AddComponentType( ecps, "GRP", 0, sizeof( GCGroupIDData ), ALIGN_OF( GCGroupIDData ), NULL, NULL, serializeGroupIDComp, deserializeGroupdIDComp );
}

void gc_SwapTransformStates( GCTransformData* tf )
{
	tf->currState.pos = tf->futureState.pos;
	tf->currState.rotRad = tf->futureState.rotRad;
	tf->currState.scale = tf->futureState.scale;
}

GCTransformData gc_CreateTransformPos( Vector2 pos )
{
	return gc_CreateTransformPosRotScale( pos, 0.0f, VEC2_ONE );
}

GCTransformData gc_CreateTransformPosRot( Vector2 pos, float rotRad )
{
	return gc_CreateTransformPosRotScale( pos, rotRad, VEC2_ONE );
}

GCTransformData gc_CreateTransformPosScale( Vector2 pos, Vector2 scale )
{
	return gc_CreateTransformPosRotScale( pos, 0.0f, scale );
}

GCTransformData gc_CreateTransformPosRotScale( Vector2 pos, float rotRad, Vector2 scale )
{
	GCTransformData data;
	data.parentID = data.firstChildID = data.nextSiblingID = INVALID_ENTITY_ID;
	data.lastMatVal = INFINITY;
	mat3_CreateTransform( &pos, rotRad, &scale, &data.mat );
	data.currState.pos = data.futureState.pos = pos;
	data.currState.rotRad = data.futureState.rotRad = rotRad;
	data.currState.scale = data.futureState.scale = scale;
	return data;
}

static void generalComponentHandleSwitchImg( void* data, int imgID, const Vector2* offset )
{
	SDL_assert( data != NULL );

	AnimSpriteHandlerData* handlerData = (AnimSpriteHandlerData*)data;

	GCSpriteData* spriteData = NULL;
	if( !ecps_GetComponentFromEntityByID( handlerData->ecps, handlerData->id, gcSpriteCompID, &spriteData ) ) {
		SDL_assert_always( false && "Unable to retrieve sprite component." );
		return;
	}

	spriteData->img = imgID;
	// TODO: handle the offset, probably include it in the sprite data
}

static void generalComponentHandleSetAABCollider( void* data, int colliderID, float centerX, float centerY, float width, float height )
{
	SDL_assert_always( false && "Not implemented yet." );
}

static void generalComponentHandleSetCircleCollider( void* data, int colliderID, float centerX, float centerY, float radius )
{
	SDL_assert_always( false && "Not implemented yet." );
}

static void generalComponentHandleDeactivateCollider( void* data, int colliderID )
{
	SDL_assert_always( false && "Not implemented yet." );
}

GCAnimatedSpriteData gc_CreateDefaultAnimSpriteData( )
{
	GCAnimatedSpriteData data;

	data.playSpeedScale = 1.0f;
	data.playingAnim.animation = NULL;
	data.playingAnim.timePassed = 0.0f;
	data.eventHandler.data = NULL;

	data.eventHandler.handleSwitchImg = generalComponentHandleSwitchImg;
	data.eventHandler.handleSetAABCollider = generalComponentHandleSetAABCollider;
	data.eventHandler.handleSetCircleCollider = generalComponentHandleSetCircleCollider;
	data.eventHandler.handleDeactivateCollider = generalComponentHandleDeactivateCollider;
	data.eventHandler.handleAnimationCompleted = NULL; // this will need to be set up be the calling code as there is (currently) no general way to handle this

	return data;
}

void gc_PlayAnim( ECPS* ecps, EntityID id, SpriteAnimation* anim )
{
	SDL_assert( ecps != NULL );
	SDL_assert( anim != NULL );

	GCAnimatedSpriteData* animSprData = NULL;
	if( !ecps_GetComponentFromEntityByID( ecps, id, gcAnimSpriteCompID, &animSprData ) )
	{
		SDL_assert( false && "Attempting to play an animation on an entity that has no animation component." );
		return;
	}

	AnimSpriteHandlerData data;
	data.ecps = ecps;
	data.id = id;
	animSprData->eventHandler.data = &data;
	sprAnim_StartAnim( &animSprData->playingAnim, anim, &animSprData->eventHandler );
	animSprData->eventHandler.data = NULL;
}

void gc_PlayAnimByID( ECPS* ecps, EntityID id, int animID )
{
	SpriteAnimation* anim = sprAnim_GetFromID( animID );
	if( anim == NULL ) {
		llog( LOG_ERROR, "Attempting to play invalid animation on entity." );
		return;
	}

	gc_PlayAnim( ecps, id, anim );
}

GeneralCallback createSourceCallback( TrackedCallback callback )
{
	GeneralCallback gc;
	gc.type = CBT_SOURCE;
	gc.source.callback = callback;
	return gc;
}

GeneralCallback createScriptCallback( const char* callback )
{
	GeneralCallback gc;
	gc.type = CBT_LUA;
	gc.script.callback = createStringCopy( callback );
	return gc;
}

void callGeneralCallback( GeneralCallback* callback, ECPS* ecps, Entity* entity )
{
	SDL_assert( callback != NULL );
	SDL_assert( ecps != NULL );
	SDL_assert( entity != NULL );

	if( callback->type == CBT_SOURCE ) {
		if( callback->source.callback != NULL ) callback->source.callback( ecps, entity );
	} else if( callback->type == CBT_LUA ) {
		EntityID id = entity->id;
		if( callback->script.callback != NULL ) xLua_CallLuaFunction( callback->script.callback, "i|", (int)id );
	} else {
		llog( LOG_ERROR, "Unknown callback type." );
	}
}

void cleanUpGeneralCallback( GeneralCallback* callback )
{
	SDL_assert( callback != NULL );
	if( callback->type == CBT_LUA ) {
		mem_Release( callback->script.callback );
	}
}