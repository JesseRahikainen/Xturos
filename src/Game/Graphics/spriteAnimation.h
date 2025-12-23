#ifndef SPRITE_ANIMATION_H
#define SPRITE_ANIMATION_H

#include <stdint.h>
#include <stddef.h>

#include "Math/vector2.h"

#define INVALID_ANIM_FRAME UINT32_MAX
#define INVALID_ANIM_EVENT_IDX UINT32_MAX

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
	char* frameName;
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
	char* spriteSheetFile;
	float fps;
	uint32_t durationFrames;
	bool loops;
	AnimEvent* sbEvents; // TODO: Sort by frame.
	int spriteSheetPackageID;
	int loadCount;
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
bool sprAnim_LoadAssociatedData( SpriteAnimation* anim );
void sprAnim_AddEvent( SpriteAnimation* anim, AnimEvent evt );
void sprAnim_Clean( SpriteAnimation* anim );
void sprAnim_ProcessFrames( SpriteAnimation* anim, AnimEventHandler* handler, uint32_t frame );

float sprAnim_GetTotalTime( SpriteAnimation* anim );
uint32_t sprAnim_FrameAtTime( SpriteAnimation* anim, float time );

// remove all frames that are after the passed in frame
void sprAnim_RemoveEventsPostFrame( SpriteAnimation* anim, size_t* watchedEvent, uint32_t frame );

// save to external file, assumes the image ids have been properly set for the animation
bool sprAnim_Save( const char* fileName, SpriteAnimation* anim );

// load from an external file, these won't load the sprite sheet or set the image ids to use
//  call sprAnim_LoadAssociatedData after this to load the sprite sheets and set the image ids
bool sprAnim_Load( const char* fileName, SpriteAnimation* anim );

// loads from an external file and all the sprite sheets as well
bool sprAnim_FullLoad( const char* fileName, SpriteAnimation* anim );

//************************************
// Playing animation.
typedef struct {
	SpriteAnimation* animation;
	float timePassed;
} PlayingSpriteAnimation;

void sprAnim_StartAnim( PlayingSpriteAnimation* playingAnim, SpriteAnimation* anim, AnimEventHandler* handler );
void sprAnim_ProcessAnim( PlayingSpriteAnimation* playingAnim, AnimEventHandler* handler, float dt );
uint32_t sprAnim_GetCurrentFrame( PlayingSpriteAnimation* playingAnim );

//************************************
// some general querying functionality

// returns the index of the next event from a starting frame that's a set image event
uint32_t sprAnim_NextImageEvent( SpriteAnimation* anim, uint32_t startingFrame, bool forward, bool inclusive );

//************************************
// treating the animation as a resource, allowing us to better manage it and access it from script
bool sprAnim_Init( void );
int sprAnim_LoadAsResource( const char* fileName );
SpriteAnimation* sprAnim_GetFromID( int id );
int sprAnim_GetAsID( SpriteAnimation* sprAnim );
void sprAnim_UnloadFromID( int id );

// for right now we'll assume playing animations will be handled primarily by the ecps, will just need to start the playing
int sprAnim_StartAnimFromID( PlayingSpriteAnimation* playingAnim, int animID, AnimEventHandler* handler );

#endif // inclusion guard