#ifndef GENERAL_COMPONENTS_H
#define GENERAL_COMPONENTS_H

#include <stdint.h>

#include "System/ECPS/entityComponentProcessSystem.h"
#include "Math/vector2.h"
#include "Graphics/color.h"
#include "tween.h"
#include "collisionDetection.h"

// bools will have to become uint8_t to ensure they're all the same size
// int ids will have to change to int32_t or uint32_t
// all other ints should be converted to uint32_t as well
// size_t should be converted to uin32_t or uint64_t as needed
// enums will also need to use a fixed integer storage instead of the enum directly

// general use components that are shared between games
typedef struct {
	Vector2 currPos;
	Vector2 futurePos;
} GCPosData;
extern ComponentID gcPosCompID;

typedef struct {
	Color currClr;
	Color futureClr;
} GCColorData;
extern ComponentID gcClrCompID;

typedef struct {
	float currRot;
	float futureRot;
} GCRotData;
extern ComponentID gcRotCompID;

typedef struct {
	Vector2 currScale;
	Vector2 futureScale;
} GCScaleData;
extern ComponentID gcScaleCompID;

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
typedef struct {
	Vector2 collisionHalfDim;
	uint32_t camFlags;
	int32_t priority;
	void ( *overResponse )( ECPS* ecps, Entity* btn );
	void ( *leaveResponse )( ECPS* ecps, Entity* btn );
	void ( *pressResponse )( ECPS* ecps, Entity* btn );
	void ( *releaseResponse )( ECPS* ecps, Entity* btn );
} GCPointerResponseData;
extern ComponentID gcPointerResponseCompID;

// a flag used to indicate that an entity should be destroyed when next possible
extern ComponentID gcCleanUpFlagCompID;

// for right now all text will be drawn centered
typedef struct {
	uint32_t camFlags;
	int8_t depth;
	const uint8_t* text;
	int fontID;
	Color clr; // assuming any color components aren't for this, need to find a better way to do this
	float pixelSize;
} GCTextData;
extern ComponentID gcTextCompID;

// just a flag you can add to an entity that you can test for later
extern ComponentID gcWatchCompID;

// for entities that are attached to another entity
typedef struct {
	EntityID parentID;
	Vector2 offset;
} GCMountedPosOffset;
extern ComponentID gcMountedPosOffsetCompID;

// for tweening values
typedef struct {
	float duration;
	float timePassed;
	float preDelay;
	float postDelay;
	float startState;
	float endState;
	EaseFunc easing;
} GCFloatTweenData;
extern ComponentID gcAlphaTweenCompID;

typedef struct {
	float duration;
	float timePassed;
	float preDelay;
	float postDelay;
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

void gc_ColliderDataToCollider( GCColliderData* colliderData, GCPosData* posData, Collider* outCollider );

// attaches the child entity to the parent entity, use the existing positions to calculate the offset
void gc_MountEntity( ECPS* ecps, EntityID parentID, EntityID childID );

void gc_WatchEntity( ECPS* ecps, EntityID id );
void gc_UnWatchEntity( ECPS* ecps, EntityID id );

void gc_Register( ECPS* ecps );

#endif // inclusion guard