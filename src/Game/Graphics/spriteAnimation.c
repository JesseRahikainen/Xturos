#include "spriteAnimation.h"

#include "System/platformLog.h"
#include "Graphics/images.h"
#include "Graphics/imageSheets.h"
#include "Utils/stretchyBuffer.h"
#include "Math/mathUtil.h"

// Helpers for creating events to add to the animation.
AnimEvent createEvent_SetImage( uint32_t frame, const char* frameName, const Vector2* offset )
{
	SDL_assert( offset != NULL );

	AnimEvent evt;

	evt.base.type = AET_SWITCH_IMAGE;
	evt.base.frame = frame;

	evt.switchImg.frameName = frameName;
	evt.switchImg.offset = *offset;

	return evt;
}

AnimEvent createEvent_SetCircleCollider( uint32_t frame, int colliderID, float centerX, float centerY, float radius )
{
	AnimEvent evt;

	evt.base.type = AET_SET_CIRCLE_COLLIDER;
	evt.base.frame = frame;

	evt.setCircleCollider.colliderID = colliderID;
	evt.setCircleCollider.centerX = centerX;
	evt.setCircleCollider.centerY = centerY;
	evt.setCircleCollider.radius = radius;

	return evt;
}

AnimEvent createEvent_SetAABCollider( uint32_t frame, int colliderID, float centerX, float centerY, float width, float height )
{
	AnimEvent evt;

	evt.base.type = AET_SET_AAB_COLLIDER;
	evt.base.frame = frame;

	evt.setAABCollider.colliderID = colliderID;
	evt.setAABCollider.centerX = centerX;
	evt.setAABCollider.centerY = centerY;
	evt.setAABCollider.width = width;
	evt.setAABCollider.height = height;

	return evt;
}

AnimEvent createEvent_DeactivateCollider( uint32_t frame, int colliderID )
{
	AnimEvent evt;

	evt.base.type = AET_DEACTIVATE_COLLIDER;
	evt.base.frame = frame;

	evt.deactivateCollider.colliderID = colliderID;

	return evt;
}

void processAnimationEvent_SetImage( AnimEvent_SwitchImage* evt, AnimEventHandler* handler )
{
	SDL_assert( handler != NULL );
	SDL_assert( evt != NULL );

	if( handler->handleSwitchImg != NULL ) {
		handler->handleSwitchImg( handler->data, evt->imgID, &(evt->offset) );
	}
}

void processAnimationEvent_SetAABCollider( AnimEvent_SetAABCollider* evt, AnimEventHandler* handler )
{
	SDL_assert( handler != NULL );
	SDL_assert( evt != NULL );

	if( handler->handleSetAABCollider != NULL ) {
		handler->handleSetAABCollider( handler->data, evt->colliderID, evt->centerX, evt->centerY, evt->width, evt->height );
	}
}

void processAnimationEvent_SetCircleCollider( AnimEvent_SetCircleCollider* evt, AnimEventHandler* handler )
{
	SDL_assert( handler != NULL );
	SDL_assert( evt != NULL );

	if( handler->handleSetCircleCollider != NULL ) {
		handler->handleSetCircleCollider( handler->data, evt->colliderID, evt->centerX, evt->centerY, evt->radius );
	}
}

void processAnimationEvent_SetDeactivedCollider( AnimEvent_DeactivateCollider* evt, AnimEventHandler* handler )
{
	SDL_assert( handler != NULL );
	SDL_assert( evt != NULL );

	if( handler->handleDeactivateCollider != NULL ) {
		handler->handleDeactivateCollider( handler->data, evt->colliderID );
	}
}

void processAnimationEvent( AnimEvent* evt, AnimEventHandler* handler )
{
	SDL_assert( evt != NULL );

	if( handler == NULL ) return;

	switch( evt->base.type ) {
	case AET_SWITCH_IMAGE:
		processAnimationEvent_SetImage( &( evt->switchImg ), handler );
		break;
	case AET_SET_AAB_COLLIDER:
		processAnimationEvent_SetAABCollider( &( evt->setAABCollider ), handler );
		break;
	case AET_SET_CIRCLE_COLLIDER:
		processAnimationEvent_SetCircleCollider( &( evt->setCircleCollider ), handler );
		break;
	case AET_DEACTIVATE_COLLIDER:
		processAnimationEvent_SetDeactivedCollider( &( evt->deactivateCollider ), handler );
		break;
	default:
		llog( LOG_ERROR, "Unhandled animation event." );
	}
}

SpriteAnimation spriteAnimation( )
{
	SpriteAnimation newAnim;
	newAnim.durationFrames = 10;
	newAnim.fps = 10;
	newAnim.sbEvents = NULL;
	newAnim.sbSpriteSheet = NULL;
	newAnim.spriteSheetFile = NULL;
	newAnim.loops = false;
	return newAnim;
}

void sprAnim_Load( SpriteAnimation* anim )
{
	// load the sprite sheet associated with the animation
	img_LoadSpriteSheet( anim->spriteSheetFile, ST_DEFAULT, &( anim->sbSpriteSheet ) );

	// set the images associated with all SetImage events
	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		if( anim->sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
			anim->sbEvents[i].switchImg.imgID = img_GetExistingByID( anim->sbEvents[i].switchImg.frameName );
		}
	}
}

void sprAnim_AddEvent( SpriteAnimation* anim, AnimEvent evt )
{
	sb_Push( anim->sbEvents, evt );
}

void sprAnim_Clean( SpriteAnimation* anim )
{
	sb_Release( anim->sbEvents );
	img_UnloadSpriteSheet( anim->sbSpriteSheet );
}

float sprAnim_GetTotalTime( SpriteAnimation* anim )
{
	if( FLT_EQ( anim->fps, 0.0f ) ) return 0.0f;
	return anim->durationFrames / anim->fps;
}

uint32_t sprAnim_FrameAtTime( SpriteAnimation* anim, float time )
{
	uint32_t frame = (uint32_t)( time * anim->fps );
	return frame;
}

void sprAnim_RemoveEventsPostFrame( SpriteAnimation* anim, size_t* watchedEvent, uint32_t frame )
{
	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		if( anim->sbEvents[i].base.frame > frame ) {
			sb_Remove( anim->sbEvents, i );
			if( i == *watchedEvent ) {
				*watchedEvent = SIZE_MAX;
			} else if( i < *watchedEvent ) {
				--( *watchedEvent );
			}
			--i;
		}
	}
}

void sprAnim_ProcessFrames( SpriteAnimation* anim, AnimEventHandler* handler, uint32_t frame )
{
	SDL_assert( anim != NULL );

	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		if( anim->sbEvents[i].base.frame == frame ) {
			processAnimationEvent( &( anim->sbEvents[i] ), handler );
		}
	}
}

void sprAnim_StartAnim( PlayingSpriteAnimation* playingAnim, SpriteAnimation* anim, AnimEventHandler* handler )
{
	playingAnim->timePassed = 0.0f;
	playingAnim->animation = anim;

	// run the initial frame
	sprAnim_ProcessFrames( anim, handler, 0 );
}

void sprAnim_ProcessAnim( PlayingSpriteAnimation* playingAnim, AnimEventHandler* handler, float dt )
{
	SDL_assert( playingAnim != NULL );

	if( playingAnim->animation == NULL ) {
		return;
	}

	float prevTime = playingAnim->timePassed;
	playingAnim->timePassed += dt;

	// see how many frames we've switched to and process them all
	uint32_t prevFrame = sprAnim_FrameAtTime( playingAnim->animation, prevTime );
	uint32_t newFrame = sprAnim_FrameAtTime( playingAnim->animation, playingAnim->timePassed );

	for( uint32_t f = prevFrame + 1; f <= newFrame; ++f ) {
		uint32_t frame = f % playingAnim->animation->durationFrames;
		sprAnim_ProcessFrames( playingAnim->animation, handler, frame );
	}

	// wrap the time if looping
	float maxTime = playingAnim->animation->durationFrames / playingAnim->animation->fps;
	if( playingAnim->timePassed > maxTime ) {
		bool loops = playingAnim->animation->loops;
		if( handler->handleAnimationCompleted != NULL ) handler->handleAnimationCompleted( handler->data, loops );
		if( loops ) {
			while( playingAnim->timePassed > maxTime ) {
				playingAnim->timePassed -= maxTime;
			}
		}
	}
}

uint32_t sprAnim_GetCurrentFrame( PlayingSpriteAnimation* playingAnim )
{
	SDL_assert( playingAnim != NULL );

	if( playingAnim->animation == NULL ) {
		return 0;
	}
	uint32_t frame = sprAnim_FrameAtTime( playingAnim->animation, playingAnim->timePassed );
	return frame;
}