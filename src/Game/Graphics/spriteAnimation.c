#include "spriteAnimation.h"

#include <stdio.h>

#include "System/platformLog.h"
#include "Graphics/images.h"
#include "Graphics/imageSheets.h"
#include "Utils/stretchyBuffer.h"
#include "Math/mathUtil.h"
#include "Others/cmp.h"
#include "Utils/helpers.h"

static const char* ioType = "sprite animation";

// Helpers for creating events to add to the animation.
AnimEvent createEvent_SetImage( uint32_t frame, const char* frameName, const Vector2* offset )
{
	SDL_assert( offset != NULL );

	AnimEvent evt;

	evt.base.type = AET_SWITCH_IMAGE;
	evt.base.frame = frame;

	evt.switchImg.frameName = createStringCopy( frameName );
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
	newAnim.spriteSheetPackageID = -1;
	newAnim.spriteSheetFile = NULL;
	newAnim.loops = false;
	newAnim.loadCount = 0;
	return newAnim;
}

bool sprAnim_LoadAssociatedData( SpriteAnimation* anim )
{
	// load the sprite sheet associated with the animation
	anim->spriteSheetPackageID = img_LoadSpriteSheet( anim->spriteSheetFile, ST_DEFAULT, NULL );
	if( anim->spriteSheetPackageID < 0 ) {
		return false;
	}

	// set the images associated with all SetImage events
	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		if( anim->sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
			anim->sbEvents[i].switchImg.imgID = img_GetExistingByID( anim->sbEvents[i].switchImg.frameName );
		}
	}

	return true;
}

void sprAnim_AddEvent( SpriteAnimation* anim, AnimEvent evt )
{
	sb_Push( anim->sbEvents, evt );
}

void sprAnim_Clean( SpriteAnimation* anim )
{
	mem_Release( anim->spriteSheetFile );
	anim->spriteSheetFile = NULL;
	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		if( anim->sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
			mem_Release( anim->sbEvents[i].switchImg.frameName );
			anim->sbEvents[i].switchImg.frameName = NULL;
		}
	}
	sb_Release( anim->sbEvents );
	img_UnloadSpriteSheet( anim->spriteSheetPackageID );
	anim->spriteSheetPackageID = -1;
}

float sprAnim_GetTotalTime( SpriteAnimation* anim )
{
	if( FLT_EQ( anim->fps, 0.0f ) ) return 0.0f;
	return anim->durationFrames / anim->fps;
}

uint32_t sprAnim_FrameAtTime( SpriteAnimation* anim, float time )
{
	uint32_t frame = (uint32_t)( time * anim->fps );
	if( anim->loops ) {
		frame = frame % anim->durationFrames;
	} else {
		frame = MIN( frame, anim->durationFrames - 1 );
	}
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

	// new frame can be past last frame, if we're not looping then we don't want to treat it as the new frame
	if( !playingAnim->animation->loops ) {
		newFrame = MIN( playingAnim->animation->durationFrames - 1, newFrame );
	}

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
	ASSERT_AND_IF_NOT( playingAnim != NULL ) return INVALID_ANIM_FRAME;
	
	if( playingAnim->animation == NULL ) {
		return INVALID_ANIM_FRAME;
	}
	uint32_t frame = sprAnim_FrameAtTime( playingAnim->animation, playingAnim->timePassed );
	return frame;
}

uint32_t sprAnim_NextImageEvent( SpriteAnimation* anim, uint32_t startingFrame, bool forward, bool inclusive )
{
	uint32_t foundEvent = INVALID_ANIM_EVENT_IDX;
	size_t bestDist = SIZE_MAX;
	uint32_t frame = startingFrame + 1;

	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		// TODO? Make this a parameter?
		if( anim->sbEvents[i].base.type != AET_SWITCH_IMAGE ) continue;

		// first make sure it's a valid position
		if( !anim->loops ) {
			// if it doesn't loop make sure we're only checking in the right direction
			if( forward ) {
				if( anim->sbEvents[i].base.frame < startingFrame ) continue;
			} else {
				if( anim->sbEvents[i].base.frame > startingFrame ) continue;
			}
		}

		// if not inclusive then exclude starting frame
		if( !inclusive && ( anim->sbEvents[i].base.frame == startingFrame ) ) continue;

		size_t dist = SIZE_MAX;
		if( forward ) {
			if( anim->sbEvents[i].base.frame < startingFrame ) {
				// if we get here we assume we're already looping, validity check above should catch it before that
				dist = anim->durationFrames - ( startingFrame - anim->sbEvents[i].base.frame );
			} else {
				dist = anim->sbEvents[i].base.frame - startingFrame;
			}
		} else {
			if( anim->sbEvents[i].base.frame < startingFrame ) {
				// if we get here we assume we're already looping, validity check above should catch it before that
				dist = anim->durationFrames - ( anim->sbEvents[i].base.frame - startingFrame );
			} else {
				dist = startingFrame - anim->sbEvents[i].base.frame;
			}
		}

		if( dist < bestDist ) {
			bestDist = dist;
			foundEvent = (uint32_t)i;
		}
	}

	return foundEvent;
}

bool sprAnim_Save( const char* fileName, SpriteAnimation* anim )
{
	bool done = false;

	ASSERT_AND_IF_NOT( fileName != NULL ) return false;
	ASSERT_AND_IF_NOT( anim != NULL ) return false;

	cmp_ctx_t cmp;
	SDL_IOStream* ioStream = openRWopsCMPFile( fileName, "wb", &cmp );
	if( ioStream == NULL ) {
		return false;
	}

	// write out all the base data first
	CMP_WRITE( &cmp, anim->fps, cmp_write_float, ioType, "fps", goto clean_up );
	CMP_WRITE( &cmp, anim->durationFrames, cmp_write_u32, ioType, "duration", goto clean_up );
	CMP_WRITE( &cmp, anim->loops, cmp_write_bool, ioType, "looping", goto clean_up );
	CMP_WRITE_STR( &cmp, anim->spriteSheetFile, ioType, "sprite sheet file name", goto clean_up );

	uint32_t numEvents = (uint32_t)sb_Count( anim->sbEvents );
	CMP_WRITE( &cmp, numEvents, cmp_write_array, ioType, "event count", goto clean_up );

	for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
		CMP_WRITE( &cmp, anim->sbEvents[i].base.frame, cmp_write_u32, ioType, "event frame", goto clean_up );

		uint32_t typeAsU32 = (uint32_t)anim->sbEvents[i].base.type;
		CMP_WRITE( &cmp, typeAsU32, cmp_write_u32, ioType, "event type", goto clean_up );

		switch( anim->sbEvents[i].base.type ) {
		case AET_SWITCH_IMAGE: {
			const char* imgID = img_GetImgStringID( anim->sbEvents[i].switchImg.imgID );
			CMP_WRITE_STR( &cmp, imgID, ioType, "image id", goto clean_up );
			CMP_WRITE( &cmp, &( anim->sbEvents[i].switchImg.offset ), vec2_Serialize, ioType, "switch image offset", goto clean_up );
		} break;
		case AET_SET_AAB_COLLIDER:
			CMP_WRITE( &cmp, anim->sbEvents[i].setAABCollider.colliderID, cmp_write_int, ioType, "AAB collider id", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setAABCollider.centerX, cmp_write_float, ioType, "AAB center x", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setAABCollider.centerY, cmp_write_float, ioType, "AAB center y", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setAABCollider.width, cmp_write_float, ioType, "AAB width", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setAABCollider.height, cmp_write_float, ioType, "AAB height", goto clean_up );
			break;
		case AET_SET_CIRCLE_COLLIDER:
			CMP_WRITE( &cmp, anim->sbEvents[i].setCircleCollider.colliderID, cmp_write_int, ioType, "circle collider id", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setCircleCollider.centerX, cmp_write_float, ioType, "circle collider center x", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setCircleCollider.centerY, cmp_write_float, ioType, "circle collider center y", goto clean_up );
			CMP_WRITE( &cmp, anim->sbEvents[i].setCircleCollider.radius, cmp_write_float, ioType, "circle collider radius", goto clean_up );
			break;
		case AET_DEACTIVATE_COLLIDER:
			CMP_WRITE( &cmp, anim->sbEvents[i].deactivateCollider.colliderID, cmp_write_int, ioType, "deactivate collider id", goto clean_up );
			break;
		default:
			llog( LOG_ERROR, "Unable to write unknown event for animation: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}
	}

	done = true;

clean_up:
	if( !SDL_CloseIO( ioStream ) ) {
		llog( LOG_ERROR, "Error closing file %s: %s", fileName, SDL_GetError( ) );
		done = false;
	}

	if( !done ) {
		// delete partially written out file
		remove( fileName );
	}
	return done;
}

bool sprAnim_Load( const char* fileName, SpriteAnimation* anim )
{
	bool done = false;

	ASSERT_AND_IF_NOT( fileName != NULL ) return false;
	ASSERT_AND_IF_NOT( anim != NULL ) return false;

	cmp_ctx_t cmp;
	SDL_IOStream* ioStream = openRWopsCMPFile( fileName, "rb", &cmp );
	if( ioStream == NULL ) {
		return false;
	}

	CMP_READ( &cmp, anim->fps, cmp_read_float, ioType, "fps", goto clean_up );
	CMP_READ( &cmp, anim->durationFrames, cmp_read_u32, ioType, "duration", goto clean_up );
	CMP_READ( &cmp, anim->loops, cmp_read_bool, ioType, "looping", goto clean_up );

	char fileNameBuffer[256];
	uint32_t fileNameBufferSize = ARRAY_SIZE( fileNameBuffer );
	CMP_READ_STR( &cmp, fileNameBuffer, fileNameBufferSize, ioType, "sprite sheet file name", goto clean_up );
	anim->spriteSheetFile = createStringCopy( fileNameBuffer );

	uint32_t numEvents;
	CMP_READ( &cmp, numEvents, cmp_read_array, ioType, "event count", goto clean_up );

	for( uint32_t i = 0; i < numEvents; ++i ) {
		AnimEvent newEvent;
		SDL_memset( &newEvent, 0, sizeof( newEvent ) );

		CMP_READ( &cmp, newEvent.base.frame, cmp_read_u32, ioType, "event frame", goto clean_up );

		uint32_t typeAsU32;
		CMP_READ( &cmp, typeAsU32, cmp_read_u32, ioType, "event type", goto clean_up );
		newEvent.base.type = (AnimEventTypes)typeAsU32;

		switch( newEvent.base.type ) {
		case AET_SWITCH_IMAGE: {
			char imageNameBuffer[256];
			uint32_t imageNameBufferSize = ARRAY_SIZE( imageNameBuffer );
			CMP_READ_STR( &cmp, imageNameBuffer, imageNameBufferSize, ioType, "image id", goto clean_up );
			newEvent.switchImg.frameName = createStringCopy( imageNameBuffer );
			CMP_READ( &cmp, newEvent.switchImg.offset, vec2_Deserialize, ioType, "switch image offset", goto clean_up );
		} break;
		case AET_SET_AAB_COLLIDER:
			CMP_READ( &cmp, newEvent.setAABCollider.colliderID, cmp_read_int, ioType, "AAB collider id", goto clean_up );
			CMP_READ( &cmp, newEvent.setAABCollider.centerX, cmp_read_float, ioType, "AAB center x", goto clean_up );
			CMP_READ( &cmp, newEvent.setAABCollider.centerY, cmp_read_float, ioType, "AAB center y", goto clean_up );
			CMP_READ( &cmp, newEvent.setAABCollider.width, cmp_read_float, ioType, "AAB width", goto clean_up );
			CMP_READ( &cmp, newEvent.setAABCollider.height, cmp_read_float, ioType, "AAB height", goto clean_up );
			break;
		case AET_SET_CIRCLE_COLLIDER:
			CMP_READ( &cmp, newEvent.setCircleCollider.colliderID, cmp_read_int, ioType, "circle collider id", goto clean_up );
			CMP_READ( &cmp, newEvent.setCircleCollider.centerX, cmp_read_float, ioType, "circle collider center x", goto clean_up );
			CMP_READ( &cmp, newEvent.setCircleCollider.centerY, cmp_read_float, ioType, "circle collider center y", goto clean_up );
			CMP_READ( &cmp, newEvent.setCircleCollider.radius, cmp_read_float, ioType, "circle collider radius", goto clean_up );
			break;
		case AET_DEACTIVATE_COLLIDER:
			CMP_READ( &cmp, newEvent.deactivateCollider.colliderID, cmp_read_int, ioType, "deactivate collider id", goto clean_up );
			break;
		default:
			llog( LOG_ERROR, "Unable to read unknown event for animation: %s", cmp_strerror( &cmp ) );
			goto clean_up;
		}

		sb_Push( anim->sbEvents, newEvent );
	}

	done = true;

clean_up:
	if( !SDL_CloseIO( ioStream ) ) {
		llog( LOG_ERROR, "Error closing file %s: %s", fileName, SDL_GetError( ) );
		done = false;
	}

	if( !done ) {
		// clean up partially loaded data
		mem_Release( anim->spriteSheetFile );
		anim->spriteSheetFile = NULL;
		for( size_t i = 0; i < sb_Count( anim->sbEvents ); ++i ) {
			if( anim->sbEvents[i].base.type == AET_SWITCH_IMAGE ) {
				mem_Release( anim->sbEvents[i].switchImg.frameName );
			}
		}
		sb_Release( anim->sbEvents );
	}
	return done;
}

bool sprAnim_FullLoad( const char* fileName, SpriteAnimation* anim )
{
	if( !sprAnim_Load( fileName, anim ) ) {
		return false;
	}

	return sprAnim_LoadAssociatedData( anim );
}

//************************************
// treating the animation as a resource, allowing us to better manage it and access it from script

#include "Utils/hashMap.h"

static HashMap loadedAnimHashMap;
static SpriteAnimation* sbLoadedAnims;

bool sprAnim_Init( void )
{
	// need to clear out some things and initialize some data here
	hashMap_Init( &loadedAnimHashMap, 32, NULL );
	sbLoadedAnims = NULL;

	return true;
}

int sprAnim_LoadAsResource( const char* fileName )
{
	int id = -1;
	// check to see if it's already loaded, if it is then just return the already loaded id and increment the count
	if( hashMap_Find( &loadedAnimHashMap, fileName, &id ) ) {
		++sbLoadedAnims[id].loadCount;
		return id;
	}

	// see if there are any open spots
	for( size_t i = 0; ( id == -1 ) && ( i < sb_Count( sbLoadedAnims ) ); ++i ) {
		if( sbLoadedAnims[i].loadCount <= 0 ) {
			id = (int)i;
		}
	}

	if( id == -1 ) {
		id = (int)sb_Count( sbLoadedAnims );
		sb_Add( sbLoadedAnims, 1 );
		sbLoadedAnims[id] = spriteAnimation( );
	}

	sprAnim_FullLoad( fileName, &( sbLoadedAnims[id] ) );
	hashMap_Set( &loadedAnimHashMap, fileName, id );
	sbLoadedAnims[id].loadCount = 1;

	return id;
}

SpriteAnimation* sprAnim_GetFromID( int id )
{
	// get the loaded animation
	if( ( id >= sb_Count( sbLoadedAnims ) ) || ( sbLoadedAnims[id].loadCount <= 0 ) ) {
		return NULL;
	}

	return &( sbLoadedAnims[id] );
}

int sprAnim_GetAsID( SpriteAnimation* sprAnim )
{
	// todo: find a better way to do this
	ASSERT_AND_IF( sprAnim == NULL ) {
		return 0xFFFFFFFF;
	}

	for( size_t i = 0; i < sb_Count( sbLoadedAnims ); ++i ) {
		if( sprAnim == &( sbLoadedAnims[i] ) ) {
			return (int)i;
		}
	}

	return 0xFFFFFFFF;
}

void sprAnim_UnloadFromID( int id )
{
	// decrement the load count, if it's zero then unload it
	SpriteAnimation* anim = sprAnim_GetFromID( id );

	if( anim == NULL ) return;
	--anim->loadCount;

	if( anim->loadCount <= 0 ) {
		sprAnim_Clean( anim );
		hashMap_RemoveFirstByValue( &loadedAnimHashMap, id );
	}
}