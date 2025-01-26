#ifndef GENERAL_COMPONENTS_H
#define GENERAL_COMPONENTS_H

#include <stdint.h>

#include "System/ECPS/entityComponentProcessSystem.h"
#include "Math/vector2.h"
#include "Math/matrix3.h"
#include "Graphics/color.h"
#include "tween.h"
#include "collisionDetection.h"
#include "Graphics/spriteAnimation.h"
#include "System/ECPS/ecps_trackedCallbacks.h"
#include "System/shared.h"

// used for callbacks that could point to source or script files
typedef struct {
	CallbackType type;
	TrackedCallback callback;
} SourceCallback;

typedef struct {
	CallbackType type;
	char* callback; // name of the Lua function to call
} ScriptCallback;

typedef union {
	CallbackType type;
	SourceCallback source;
	ScriptCallback script;
} GeneralCallback;

GeneralCallback createSourceCallback( TrackedCallback callback );
GeneralCallback createScriptCallback( const char* callback );
void callGeneralCallback( GeneralCallback* callback, ECPS* ecps, Entity* entity );
void cleanUpGeneralCallback( GeneralCallback* callback );

// general use components that are shared between games
typedef struct {
	Vector2 pos;
	Vector2 scale;
	float rotRad;
} GCTransformState;

typedef struct {
	GCTransformState currState;
	GCTransformState futureState; // all work is done on this state, currState should be considered locked (except when wanting to move without doing lerping)
	EntityID parentID;
	EntityID firstChildID;
	EntityID nextSiblingID;

	Matrix3 mat;
	float lastMatVal;
} GCTransformData;
extern ComponentID gcTransformCompID;

typedef struct {
	Color currClr;
	Color futureClr;
} GCColorData;
extern ComponentID gcClrCompID;

typedef struct {
	float currValue;
	float futureValue;
} GCFloatVal0Data;
extern ComponentID gcFloatVal0CompID;

typedef struct {
	int img;
	int8_t depth;
	uint32_t camFlags;
} GCSpriteData;
extern ComponentID gcSpriteCompID;

typedef struct {
	bool isStencil;
	int stencilID;
} GCStencilData;
extern ComponentID gcStencilCompID;

typedef struct {
	int imgs[9]; // tl, tc, tr, ml, mc, mr, bl, bc, br
	int8_t depth;
	uint32_t camFlags;
	Vector2 size;
} GC3x3SpriteData;
extern ComponentID gc3x3SpriteCompID;

// colliders, we only store the data that isn't be stored elsewhere for the collisions
//  structured similar to the collider union in the collision detection code, could be
//  sped up by caching some things, or having a backing Collider array we could use.
typedef struct {
	enum ColliderType type;
} GCBaseCollisionData;

typedef struct {
	GCBaseCollisionData base;
	Vector2 halfDim;
} GCAABCollisionData;

typedef struct {
	GCBaseCollisionData base;
	float radius;
} GCCircleCollisionData;

typedef struct {
	GCBaseCollisionData base;
	Vector2 normal;
} GCHalfSpaceCollisionData;

typedef union {
	GCBaseCollisionData base;
	GCAABCollisionData aab;
	GCCircleCollisionData circle;
	GCHalfSpaceCollisionData halfSpace;
} GCColliderData;
extern ComponentID gcColliderCompID;

// used for buttons or anything else that should be clickable
//typedef void ( *ButtonResponse )( ECPS* ecps, Entity* btn );
typedef struct {
	Vector2 collisionHalfDim;
	uint32_t camFlags;
	int32_t priority;
} GCPointerCollisionData;
extern ComponentID gcPointerCollisionCompID;

// used for buttons or clickable things
typedef struct {
	GeneralCallback overResponse;
	GeneralCallback leaveResponse;
	GeneralCallback pressResponse;
	GeneralCallback releaseResponse;
} GCPointerResponseData;
extern ComponentID gcPointerResponseCompID;

// a flag used to indicate that an entity should be destroyed when next possible
extern ComponentID gcCleanUpFlagCompID;

// for right now all text will be drawn centered
typedef struct {
	uint32_t camFlags;
	int8_t depth;
	uint8_t* text;
	int fontID;
	Color clr; // assuming any color components aren't for this, need to find a better way to do this
	float pixelSize;
	bool textIsDynamic;
} GCTextData;
extern ComponentID gcTextCompID;

// just a flag you can add to an entity that you can test for later
extern ComponentID gcWatchCompID;

// for tweening values
typedef struct {
	float duration;
	float timePassed;
	float preDelay; // normalized duration where change will start
	float postDelay; // normalized duration where change will end
	float startState;
	float endState;
	EaseFunc easing;
} GCFloatTweenData;
extern ComponentID gcAlphaTweenCompID;

typedef struct {
	float duration;
	float timePassed;
	float preDelay; // normalized duration where change will start
	float postDelay; // normalized duration where change will end
	Vector2 startState;
	Vector2 endState;
	EaseFunc easing;
} GCVec2TweenData;
extern ComponentID gcPosTweenCompID;
extern ComponentID gcScaleTweenCompID;

typedef struct {
	uint32_t groupID;
} GCGroupIDData;
extern ComponentID gcGroupIDCompID;

typedef struct {
	Vector2 currentSize;
	Vector2 futureSize;
} GCSizeData;
extern ComponentID gcSizeCompID;

typedef struct {
	float duration;
	float timePassed;
	GeneralCallback onDestroyedCallback;
} GCShortLivedData;
extern ComponentID gcShortLivedCompID;

typedef struct {
	ECPS* ecps;
	EntityID id;
} AnimSpriteHandlerData;

typedef struct {
	PlayingSpriteAnimation playingAnim;
	AnimEventHandler eventHandler;
	float playSpeedScale;
} GCAnimatedSpriteData;
extern ComponentID gcAnimSpriteCompID;

// TODO: a general UI component with size, anchors, and such

// swaps over the values in the future state of a transform to the current state. call once rendering of the transition from current to future is finished.
void gc_SwapTransformStates( GCTransformData* tf );

// create a transform given various parts of it
GCTransformData gc_CreateTransformPos( Vector2 pos );
GCTransformData gc_CreateTransformPosRot( Vector2 pos, float rotRad );
GCTransformData gc_CreateTransformPosScale( Vector2 pos, Vector2 scale );
GCTransformData gc_CreateTransformPosRotScale( Vector2 pos, float rotRad, Vector2 scale );

void gc_ColliderDataToCollider( GCColliderData* colliderData, GCTransformData* posData, Collider* outCollider );

// attaches the child entity to the parent entity, uses the existing positions to calculate the child transform
void gc_MountEntity( ECPS* ecps, EntityID parentID, EntityID childID );

// detaches the entity from it's parent
void gc_UnmountEntityFromParent( ECPS* ecps, EntityID id );

// transform helper functions
void gc_GetLerpedGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, float t, Matrix3* outMat );
void gc_GetCurrGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, Matrix3* outMat );
void gc_GetFutureGlobalMatrix( ECPS* ecps, GCTransformData* tf, const Vector2* imageOffset, Matrix3* outMat );
Vector2 gc_GetLocalPos( GCTransformData* tf );
float gc_GetLocalRotRad( GCTransformData* tf );
Vector2 gc_GetLocalScale( GCTransformData* tf );
void gc_SetTransformLocalPos( GCTransformData* tf, Vector2 pos );
void gc_SetTransformLocalRot( GCTransformData* tf, float rotRad );
void gc_SetTransformLocalVectorScale( GCTransformData* tf, Vector2 scale );
void gc_SetTransformLocalFloatScale( GCTransformData* tf, float scale );
void gc_AdjustLocalPos( GCTransformData* tf, Vector2* adj );
void gc_AdjustLocalRot( GCTransformData* tf, float adjRad );
void gc_AdjustLocalVectorScale( GCTransformData* tf, Vector2* adj );
void gc_AdjustLocalFloatScale( GCTransformData* tf, float adj );
void gc_TeleportToLocalPos( GCTransformData* tf, Vector2 pos );
void gc_TeleportToLocalRot( GCTransformData* tf, float rotRad );
void gc_TeleportToLocalVectorScale( GCTransformData* tf, Vector2 scale );
void gc_TeleportToLocalFloatScale( GCTransformData* tf, float scale );

void gc_WatchEntity( ECPS* ecps, EntityID id );
void gc_UnWatchEntity( ECPS* ecps, EntityID id );

GCAnimatedSpriteData gc_CreateDefaultAnimSpriteData( );
void gc_PlayAnim( ECPS* ecps, EntityID id, SpriteAnimation* anim );
void gc_PlayAnimByID( ECPS* ecps, EntityID id, int animID );

void gc_Register( ECPS* ecps );

#endif // inclusion guard