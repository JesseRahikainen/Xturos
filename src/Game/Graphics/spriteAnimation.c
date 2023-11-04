#include "spriteAnimation.h"

#include "System/platformLog.h"
#include "Graphics/images.h"
#include "Graphics/imageSheets.h"
#include "Utils/stretchyBuffer.h"
#include "Math/mathUtil.h"
#include "Others/cmp.h"
#include "Utils/helpers.h"

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

void sprAnim_LoadAssociatedData( SpriteAnimation* anim )
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
	mem_Release( anim->spriteSheetFile );
	anim->spriteSheetFile = NULL;
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

bool sprAnim_Save( const char* fileName, SpriteAnimation* anim )
{
	ASSERT_AND_IF( fileName != NULL ) return false;
	ASSERT_AND_IF( anim != NULL ) return false;

	cmp_ctx_t cmp;
	SDL_RWops* rwops = openRWopsCMPFile( fileName, "w", &cmp );
	if( rwops == NULL ) {
		return false;
	}

#define CMP_WRITE( val, write, desc ) \
	if( !write( &cmp, (val) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for animation: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

#define CMP_WRITE_STR( val, write, desc ) \
	if( !cmp_write_str( &cmp, (val), (uint32_t)SDL_strlen((val)) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for animation: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

	// create the sprite sheet we'll use
//#error working here, will just want to feed it a list of image ids to use, it will then extract the pixel data and use it to create one or more sprite sheets

	// write out all the base data first
	CMP_WRITE( anim->fps, cmp_write_float, "fps" );
	CMP_WRITE( anim->durationFrames, cmp_write_u32, "duration" );
	CMP_WRITE( anim->loops, cmp_write_bool, "looping" );

	uint32_t numEvents = (uint32_t)sb_Count( anim->sbEvents );
	CMP_WRITE( numEvents, cmp_write_array, "event count" );

	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		CMP_WRITE( anim->sbEvents[i].base.frame, cmp_write_u32, "event frame" );

		uint32_t typeAsU32 = (uint32_t)anim->sbEvents[i].base.type;
		CMP_WRITE( typeAsU32, cmp_write_u32, "event type" );

		switch( anim->sbEvents[i].base.type ) {
		case AET_SWITCH_IMAGE: {
			const char* imgID = img_GetImgStringID( anim->sbEvents[i].switchImg.imgID );
			CMP_WRITE_STR( imgID, cmp_write_str, "image id" );
			CMP_WRITE( &( anim->sbEvents[i].switchImg.offset ), vec2_Serialize, "switch image offset" );
		} break;
		case AET_SET_AAB_COLLIDER:
			CMP_WRITE( anim->sbEvents[i].setAABCollider.colliderID, cmp_write_int, "AAB collider id" );
			CMP_WRITE( anim->sbEvents[i].setAABCollider.centerX, cmp_write_float, "AAB center x" );
			CMP_WRITE( anim->sbEvents[i].setAABCollider.centerY, cmp_write_float, "AAB center y" );
			CMP_WRITE( anim->sbEvents[i].setAABCollider.width, cmp_write_float, "AAB width" );
			CMP_WRITE( anim->sbEvents[i].setAABCollider.height, cmp_write_float, "AAB height" );
			break;
		case AET_SET_CIRCLE_COLLIDER:
			CMP_WRITE( anim->sbEvents[i].setCircleCollider.colliderID, cmp_write_int, "circle collider id" );
			CMP_WRITE( anim->sbEvents[i].setCircleCollider.centerX, cmp_write_float, "circle collider center x" );
			CMP_WRITE( anim->sbEvents[i].setCircleCollider.centerY, cmp_write_float, "circle collider center y" );
			CMP_WRITE( anim->sbEvents[i].setCircleCollider.radius, cmp_write_float, "circle collider radius" );
			break;
		case AET_DEACTIVATE_COLLIDER:
			CMP_WRITE( anim->sbEvents[i].deactivateCollider.colliderID, cmp_write_int, "deactivate collider id" );
			break;
		default:
			llog( LOG_ERROR, "Unable to write unknown event for animation: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
	}
#undef CMP_WRITE

clean_up:

	SDL_RWclose( rwops );
	return true;
}

bool sprAnim_Load( const char* fileName, SpriteAnimation* anim )
{
	return false;
}