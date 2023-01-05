#ifndef SPRITE_ANIMATION_H
#define SPRITE_ANIMATION_H

#include <stdint.h>

#include "Math/vector2.h"

/*
* Animation tool:
*  This will be able to be used to create the animations to be used in the game. Will be stored in an external file which can be loaded and played.
*  Because of how we're doing things we'll focus on an event based approach. Each animation will be a collection of events, setting various things.
*  We'll want to be able to create an Animation. This will have a FPS and a Frame Count. It will also have a list of zero or more Events.
*/

typedef enum {
	AET_SWITCH_IMAGE,
	AET_SET_AAB_COLLIDER,
	AET_SET_CIRCLE_COLLIDER,
	AET_DEACTIVATE_COLLIDER,
	MAX_ANIM_EVENT_TYPES
} AnimEventTypes;

typedef struct {
	AnimEventTypes type;
	uint32_t frame;
} AnimEvent_Base;

typedef struct {
	AnimEvent_Base base;
	const char* frameName; // TODO: Remove this once we get the animation tool set up, or maybe not, may be useful for debugging.
	int imgID;
	Vector2 offset;
} AnimEvent_SwitchImage;

typedef struct {
	AnimEvent_Base base;
	int colliderID;
	float centerX;
	float centerY;
	float width;
	float height;
} AnimEvent_SetAABCollider;

typedef struct {
	AnimEvent_Base base;
	int colliderID;
	float centerX;
	float centerY;
	float radius;
} AnimEvent_SetCircleCollider;

typedef struct {
	AnimEvent_Base base;
	int colliderID;
} AnimEvent_DeactivateCollider;

typedef union {
	AnimEvent_Base base;
	AnimEvent_SwitchImage switchImg;
	AnimEvent_SetAABCollider setAABCollider;
	AnimEvent_SetCircleCollider setCircleCollider;
	AnimEvent_DeactivateCollider deactivateCollider;
} AnimEvent;

typedef struct {
	void* data;
	void ( *handleSwitchImg )( void* data, int imgID, const Vector2* offset );
	void ( *handleSetAABCollider )( void* data, int colliderID, float centerX, float centerY, float width, float height );
	void ( *handleSetCircleCollider )( void* data, int colliderID, float centerX, float centerY, float radius );
	void ( *handleDeactivateCollider )( void* data, int colliderID );
	void ( *handleAnimationCompleted )( void* data, bool loops );
} AnimEventHandler;

typedef struct {
	const char* spriteSheetFile; // TODO: Remove once we get the tool set up.
	float fps;
	uint32_t durationFrames;
	bool loops;
	AnimEvent* sbEvents; // TODO: Sort by frame.
	int* sbSpriteSheet;
} SpriteAnimation;

// Helpers for creating events to add to the animation.
AnimEvent createEvent_SetImage( uint32_t frame, const char* frameName, const Vector2* offset );
AnimEvent createEvent_SetCircleCollider( uint32_t frame, int colliderID, float centerX, float centerY, float radius );
AnimEvent createEvent_SetAABCollider( uint32_t frame, int colliderID, float centerX, float centerY, float width, float height );
AnimEvent createEvent_DeactivateCollider( uint32_t frame, int colliderID );

void processAnimationEvent_SetImage( AnimEvent_SwitchImage* evt, AnimEventHandler* handler );
void processAnimationEvent_SetAABCollider( AnimEvent_SetAABCollider* evt, AnimEventHandler* handler );
void processAnimationEvent_SetCircleCollider( AnimEvent_SetCircleCollider* evt, AnimEventHandler* handler );
void processAnimationEvent_SetDeactivedCollider( AnimEvent_DeactivateCollider* evt, AnimEventHandler* handler );
void processAnimationEvent( AnimEvent* evt, AnimEventHandler* handler );

SpriteAnimation spriteAnimation( );
void sprAnim_Load( SpriteAnimation* anim );
void sprAnim_AddEvent( SpriteAnimation* anim, AnimEvent evt );
void sprAnim_Clean( SpriteAnimation* anim );
void sprAnim_ProcessFrames( SpriteAnimation* anim, AnimEventHandler* handler, uint32_t frame );

float sprAnim_GetTotalTime( SpriteAnimation* anim );
uint32_t sprAnim_FrameAtTime( SpriteAnimation* anim, float time );

// remove all frames that are after the passed in frame
void sprAnim_RemoveEventsPostFrame( SpriteAnimation* anim, size_t* watchedEvent, uint32_t frame );

//************************************
// Playing animation.
typedef struct {
	SpriteAnimation* animation;
	float timePassed;
} PlayingSpriteAnimation;

void sprAnim_StartAnim( PlayingSpriteAnimation* playingAnim, SpriteAnimation* anim, AnimEventHandler* handler );
void sprAnim_ProcessAnim( PlayingSpriteAnimation* playingAnim, AnimEventHandler* handler, float dt );
uint32_t sprAnim_GetCurrentFrame( PlayingSpriteAnimation* playingAnim );

#endif // inclusion guard