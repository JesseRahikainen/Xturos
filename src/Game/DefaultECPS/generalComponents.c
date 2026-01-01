#include "generalComponents.h"

#include <SDL3/SDL_assert.h>
#include <math.h>

#include "Utils/helpers.h"
#include "System/platformLog.h"
#include "System/ECPS/ecps_fileSerialization.h"
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
static bool serializeGeneralCallback( Serializer* s, GeneralCallback* callback, const char* type, const char* desc )
{
	ASSERT_AND_IF_NOT( callback != NULL ) return false;
	ASSERT_AND_IF_NOT( s != NULL ) return false;

	SERIALIZE_ENUM( s, type, "callbackType", callback->type, CallbackType, return false );
	if( callback->type == CBT_SOURCE ) {
		SERIALIZE_CHECK( s->trackedECPSCallback( s, "sourceFuncID", &( callback->source.callback ) ), type, desc, return false );
	} else if( callback->type == CBT_LUA ) {
		SERIALIZE_CHECK( s->cString( s, "scriptFunc", &( callback->script.callback ) ), type, "scriptFunc", return false );
	} else {
		llog( LOG_ERROR, "Unknown callback type when saving %s for %s.", desc, type );
		return false;
	}

	return true;
}

//====
static bool serializeColorComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCColorData* clr = (GCColorData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCColorData", "starting", return false );
	SERIALIZE_CHECK( clr_Serialize( s, "currClr", &( clr->currClr ) ), "GCColorData", "currClr", return false );
	SERIALIZE_CHECK( clr_Serialize( s, "futureClr", &( clr->futureClr ) ), "GCColorData", "futureClr", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCColorData", "ending", return false );

	return true;
}

//====
static bool serializeSpriteComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCSpriteData* sprite = (GCSpriteData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCSpriteData", "starting", return false );
	SERIALIZE_CHECK( s->u32( s, "camFlags", &( sprite->camFlags ) ), "GCSpriteData", "camFlags", return false );
	SERIALIZE_CHECK( s->s8( s, "depth", &( sprite->depth ) ), "GCSpriteData", "depth", return false );
	SERIALIZE_CHECK( s->imageID( s, "img", &( sprite->img ) ), "GCSpriteData", "img", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCSpriteData", "ending", return false );

	return true;
}

//====
static bool serialize3x3Comp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GC3x3SpriteData* sprite = (GC3x3SpriteData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GC3x3SpriteData", "starting", return false );
	SERIALIZE_CHECK( s->u32( s, "camFlags", &( sprite->camFlags ) ), "GC3x3SpriteData", "camFlags", return false );
	SERIALIZE_CHECK( s->s8( s, "depth", &( sprite->depth ) ), "GC3x3SpriteData", "depth", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "size", &( sprite->size ) ), "GC3x3SpriteData", "size", return false );
	SERIALIZE_CHECK( s->imageID( s, "img", &( sprite->img ) ), "GC3x3SpriteData", "img", return false );
	SERIALIZE_CHECK( s->u32( s, "topBorder", &( sprite->topBorder ) ), "GC3x3SpriteData", "topBorder", return false );
	SERIALIZE_CHECK( s->u32( s, "bottomBorder", &( sprite->bottomBorder ) ), "GC3x3SpriteData", "bottomBorder", return false );
	SERIALIZE_CHECK( s->u32( s, "leftBorder", &( sprite->leftBorder ) ), "GC3x3SpriteData", "leftBorder", return false );
	SERIALIZE_CHECK( s->u32( s, "rightBorder", &( sprite->rightBorder ) ), "GC3x3SpriteData", "rightBorder", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GC3x3SpriteData", "ending", return false );

	return true;
}

//====
static bool serializeCollComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCColliderData* coll = (GCColliderData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCColliderData", "starting", return false );
	SERIALIZE_ENUM( s, "GCColliderData", "callbackType", coll->base.type, enum ColliderType, return false );
	switch( coll->base.type ) {
		case CT_AAB:
			SERIALIZE_CHECK( vec2_Serialize( s, "halfDim", &( coll->aab.halfDim ) ), "GCColliderData", "halfDim", return false );
			break;
		case CT_CIRCLE:
			SERIALIZE_CHECK( s->flt( s, "radius", &( coll->circle.radius ) ), "GCColliderData", "radius", return false );
			break;
		case CT_HALF_SPACE:
			SERIALIZE_CHECK( vec2_Serialize( s, "normal", &( coll->halfSpace.normal ) ), "GCColliderData", "normal", return false );
			break;
		default:
			ASSERT_ALWAYS( "Attempting to serialize unsupported collision type." );
			return false;
	}
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCColliderData", "ending", return false );

	return true;
}

//====
static bool serializeClickCollisionComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCPointerCollisionData* click = (GCPointerCollisionData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCClickCollisionData", "starting", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "halfDim", &( click->collisionHalfDim ) ), "GCClickCollisionData", "collisionHalfDim", return false );
	SERIALIZE_CHECK( s->u32( s, "camFlags", &( click->camFlags ) ), "GCClickCollisionData", "camFlags", return false );
	SERIALIZE_CHECK( s->s32( s, "priority", &( click->priority ) ), "GCClickCollisionData", "priority", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCClickCollisionData", "ending", return false );

	return true;
}

//====
static bool serializeClickFuncResponseComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCPointerResponseData* response = (GCPointerResponseData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCPointerResponseData", "starting", return false );
	if( !serializeGeneralCallback( s, &response->overResponse, "GCPointerResponseData", "overResponse" ) ) return false;
	if( !serializeGeneralCallback( s, &response->leaveResponse, "GCPointerResponseData", "leaveResponse" ) ) return false;
	if( !serializeGeneralCallback( s, &response->pressResponse, "GCPointerResponseData", "pressResponse" ) ) return false;
	if( !serializeGeneralCallback( s, &response->releaseResponse, "GCPointerResponseData", "releaseResponse" ) ) return false;
	SERIALIZE_CHECK( s->endStructure( s, "" ), "CPointerResponseData", "ending", return false );

	return true;
}

//====
static bool serializeTextComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

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

//====
static bool serializeVal0Comp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCFloatVal0Data* val = (GCFloatVal0Data*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCFloatVal0Data", "starting", return false );
	SERIALIZE_CHECK( s->flt( s, "currValue", &( val->currValue ) ), "GCFloatVal0Data", "currValue", return false );
	SERIALIZE_CHECK( s->flt( s, "futureValue", &( val->futureValue ) ), "GCFloatVal0Data", "futureValue", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCFloatVal0Data", "ending", return false );

	return true;
}

//====
static bool serializeStencilComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCStencilData* val = (GCStencilData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCStencilData", "starting", return false );
	SERIALIZE_CHECK( s->boolean( s, "isStencil", &( val->isStencil ) ), "GCStencilData", "isStencil", return false );
	SERIALIZE_CHECK( s->s8( s, "stencilID", &( val->stencilID ) ), "GCStencilData", "stencilID", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCStencilData", "ending", return false );

	return true;
}

//====
static bool serializeVec2TweenComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCVec2TweenData* val = (GCVec2TweenData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCStencilData", "starting", return false );
	SERIALIZE_CHECK( s->flt( s, "duration", &( val->duration ) ), "GCVec2TweenData", "duration", return false );
	SERIALIZE_CHECK( s->flt( s, "timePassed", &( val->timePassed ) ), "GCVec2TweenData", "timePassed", return false );
	SERIALIZE_CHECK( s->flt( s, "preDelay", &( val->preDelay ) ), "GCVec2TweenData", "preDelay", return false );
	SERIALIZE_CHECK( s->flt( s, "postDelay", &( val->postDelay ) ), "GCVec2TweenData", "postDelay", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "startState", &( val->startState ) ), "GCVec2TweenData", "startState", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "endState", &( val->endState ) ), "GCVec2TweenData", "endState", return false );
	SERIALIZE_CHECK( s->trackedEaseFunc( s, "easing", &( val->easing ) ), "GCVec2TweenData", "easing", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCVec2TweenData", "ending", return false );

	return true;
}

//====
static bool serializeFloatTweenComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCFloatTweenData* val = (GCFloatTweenData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCFloatTweenData", "starting", return false );
	SERIALIZE_CHECK( s->flt( s, "duration", &( val->duration ) ), "GCFloatTweenData", "duration", return false );
	SERIALIZE_CHECK( s->flt( s, "timePassed", &( val->timePassed ) ), "GCFloatTweenData", "timePassed", return false );
	SERIALIZE_CHECK( s->flt( s, "preDelay", &( val->preDelay ) ), "GCFloatTweenData", "preDelay", return false );
	SERIALIZE_CHECK( s->flt( s, "postDelay", &( val->postDelay ) ), "GCFloatTweenData", "postDelay", return false );
	SERIALIZE_CHECK( s->flt( s, "startState", &( val->startState ) ), "GCFloatTweenData", "startState", return false );
	SERIALIZE_CHECK( s->flt( s, "endState", &( val->endState ) ), "GCFloatTweenData", "endState", return false );
	SERIALIZE_CHECK( s->trackedEaseFunc( s, "easing", &( val->easing ) ), "GCFloatTweenData", "easing", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCFloatTweenData", "ending", return false );

	return true;
}

//====
static bool serializeGroupIDComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCGroupIDData* val = (GCGroupIDData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCGroupIDData", "starting", return false );
	SERIALIZE_CHECK( s->u32( s, "groupID", &( val->groupID ) ), "GCGroupIDData", "groupID", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCGroupIDData", "ending", return false );

	return true;
}

//====
static bool serializeTranformComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCTransformData* val = (GCTransformData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCTransformData", "starting", return false );

	SERIALIZE_CHECK( vec2_Serialize( s, "currPos", &( val->currState.pos ) ), "GCTransformData", "currPos", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "currScale", &( val->currState.scale ) ), "GCTransformData", "currScale", return false );
	SERIALIZE_CHECK( s->flt( s, "currRotRad", &( val->currState.rotRad ) ), "GCTransformData", "currRotRad", return false );

	SERIALIZE_CHECK( vec2_Serialize( s, "futurePos", &( val->futureState.pos ) ), "GCTransformData", "futurePos", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "futureScale", &( val->futureState.scale ) ), "GCTransformData", "futureScale", return false );
	SERIALIZE_CHECK( s->flt( s, "futureRotRad", &( val->futureState.rotRad ) ), "GCTransformData", "futureRotRad", return false );

	SERIALIZE_CHECK( s->entityID( s, "parentID", &( val->parentID ) ), "GCTransformData", "parentID", return false );
	SERIALIZE_CHECK( s->entityID( s, "firstChildID", &( val->firstChildID ) ), "GCTransformData", "firstChildID", return false );
	SERIALIZE_CHECK( s->entityID( s, "nextSiblingID", &( val->nextSiblingID ) ), "GCTransformData", "nextSiblingID", return false );

	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCTransformData", "ending", return false );

	return true;
}

//=====================================================================================================

static bool serializeSizeComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCSizeData* val = (GCSizeData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCSizeData", "starting", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "currentSize", &( val->currentSize ) ), "GCSizeData", "currentSize", return false );
	SERIALIZE_CHECK( vec2_Serialize( s, "futureSize", &( val->futureSize ) ), "GCSizeData", "futureSize", return false );
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCSizeData", "ending", return false );

	return true;
}

//====
static bool serializeShortLivedComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCShortLivedData* val = (GCShortLivedData*)data;

	SERIALIZE_CHECK( s->startStructure( s, "" ), "GCShortLivedData", "starting", return false );
	SERIALIZE_CHECK( s->flt( s, "duration", &( val->duration ) ), "GCShortLivedData", "duration", return false );
	SERIALIZE_CHECK( s->flt( s, "timePassed", &( val->duration ) ), "GCShortLivedData", "timePassed", return false );
	if( !serializeGeneralCallback( s, &( val->onDestroyedCallback ), "GCShortLivedData", "onDestroyedCallback" ) ) return false;
	SERIALIZE_CHECK( s->endStructure( s, "" ), "GCShortLivedData", "ending", return false );

	return false;
}

//====
static bool serializeAnimatedSpriteComp( Serializer* s, void* data )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	GCAnimatedSpriteData* val = (GCAnimatedSpriteData*)data;

	// the playingAnim and eventHandler are going to be very difficult to serialize
	//PlayingSpriteAnimation playingAnim;
	//AnimEventHandler eventHandler;
	//float playSpeedScale;

	ASSERT_ALWAYS( "serializeAnimatedSpriteComp: Not implemented yet." );

	return false;
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
	case CT_AAB:
		outCollider->type = CT_AAB;
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
	gcTransformCompID = ecps_AddComponentType( ecps, "GC_TF", 0, sizeof( GCTransformData ), ALIGN_OF( GCTransformData ), transformCleanUp, NULL, serializeTranformComp );
	gcClrCompID = ecps_AddComponentType( ecps, "GC_CLR", 0, sizeof( GCColorData ), ALIGN_OF( GCColorData ), NULL, NULL, serializeColorComp );
	gcSpriteCompID = ecps_AddComponentType( ecps, "GC_SPRT", 0, sizeof( GCSpriteData ), ALIGN_OF( GCSpriteData ), NULL, NULL, serializeSpriteComp );
	gc3x3SpriteCompID = ecps_AddComponentType( ecps, "GC_3x3", 0, sizeof( GC3x3SpriteData ), ALIGN_OF( GC3x3SpriteData ), NULL, NULL, serialize3x3Comp );
	gcColliderCompID = ecps_AddComponentType( ecps, "GC_COLL", 0, sizeof( GCColliderData ), ALIGN_OF( GCColliderData ), NULL, NULL, serializeCollComp );
	gcPointerCollisionCompID = ecps_AddComponentType( ecps, "GC_CLICK", 0, sizeof( GCPointerCollisionData ), ALIGN_OF( GCPointerCollisionData ), NULL, NULL, serializeClickCollisionComp );
	gcPointerResponseCompID = ecps_AddComponentType( ecps, "GC_CLK", 0, sizeof( GCPointerResponseData ), ALIGN_OF( GCPointerResponseData ), pointerScriptCleanUp, NULL, serializeClickFuncResponseComp );
	gcCleanUpFlagCompID = ecps_AddComponentType( ecps, "GC_DEAD", 0, 0, 0, NULL, NULL, NULL );
	gcTextCompID = ecps_AddComponentType( ecps, "GC_TXT", 0, sizeof( GCTextData ), ALIGN_OF( GCTextData ), textCleanUp, NULL, serializeTextComp );
	gcFloatVal0CompID = ecps_AddComponentType( ecps, "GC_VAL0", 0, sizeof( GCFloatVal0Data ), ALIGN_OF( GCFloatVal0Data ), NULL, NULL, serializeVal0Comp );
	gcWatchCompID = ecps_AddComponentType( ecps, "GC_WATCH", 0, 0, 0, NULL, NULL, NULL );
	gcStencilCompID = ecps_AddComponentType( ecps, "GC_STENCIL", 0, sizeof( GCStencilData ), ALIGN_OF( GCStencilData ), NULL, NULL, serializeStencilComp );
	gcSizeCompID = ecps_AddComponentType( ecps, "GC_SIZE", 0, sizeof( GCSizeData ), ALIGN_OF( GCSizeData ), NULL, NULL, serializeSizeComp );

	gcPosTweenCompID = ecps_AddComponentType( ecps, "P_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp );
	gcScaleTweenCompID = ecps_AddComponentType( ecps, "S_TWN", 0, sizeof( GCVec2TweenData ), ALIGN_OF( GCVec2TweenData ), NULL, NULL, serializeVec2TweenComp );
	gcAlphaTweenCompID = ecps_AddComponentType( ecps, "A_TWN", 0, sizeof( GCFloatTweenData ), ALIGN_OF( GCFloatTweenData ), NULL, NULL, serializeFloatTweenComp );
	gcShortLivedCompID = ecps_AddComponentType( ecps, "GC_LIFE", 0, sizeof( GCShortLivedData ), ALIGN_OF( GCShortLivedData ), NULL, NULL, serializeShortLivedComp );
	gcAnimSpriteCompID = ecps_AddComponentType( ecps, "ANIM_SPR", 0, sizeof( GCAnimatedSpriteData ), ALIGN_OF( GCAnimatedSpriteData ), NULL, NULL, serializeAnimatedSpriteComp );

	gcGroupIDCompID = ecps_AddComponentType( ecps, "GRP", 0, sizeof( GCGroupIDData ), ALIGN_OF( GCGroupIDData ), NULL, NULL, serializeGroupIDComp );
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

static void generalComponentHandleSwitchImg( void* data, ImageID imgID, const Vector2* offset )
{
	SDL_assert( data != NULL );

	AnimSpriteHandlerData* handlerData = (AnimSpriteHandlerData*)data;

	GCSpriteData* spriteData = NULL;
	if( !ecps_GetComponentFromEntityByID( handlerData->ecps, handlerData->id, gcSpriteCompID, &spriteData ) ) {
		ASSERT_ALWAYS( "Unable to retrieve sprite component." );
		return;
	}

	spriteData->img = imgID;
	// TODO: handle the offset, probably include it in the sprite data
}

static void generalComponentHandleSetAABCollider( void* data, int32_t colliderID, float centerX, float centerY, float width, float height )
{
	ASSERT_ALWAYS( "Not implemented yet." );
}

static void generalComponentHandleSetCircleCollider( void* data, int32_t colliderID, float centerX, float centerY, float radius )
{
	ASSERT_ALWAYS( "Not implemented yet." );
}

static void generalComponentHandleDeactivateCollider( void* data, int32_t colliderID )
{
	ASSERT_ALWAYS( "Not implemented yet." );
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