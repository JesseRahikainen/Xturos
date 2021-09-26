#include "messageBroadcast.h"

#include <SDL_assert.h>

#include "Utils/stretchyBuffer.h"

typedef struct {
	MessageResponse* sbResponses;
} MessageListener;

static MessageListener* sbListeners = NULL;

// register a message listener, you can register the same listener more than once
void mb_RegisterListener( const MessageID message, MessageResponse response )
{
	SDL_assert( response != NULL );

	// expand if necessary
	if( message >= sb_Count( sbListeners ) ) {
		size_t diff = ( message - sb_Count( sbListeners ) ) + 1;
		MessageListener* start = sb_Add( sbListeners, diff );
		for( size_t i = 0; i < diff; ++i, ++start ) {
			start->sbResponses = NULL;
		}
	}

	sb_Push( sbListeners[message].sbResponses, response );
}

// unregisters the listener for the message, will unregister all if there are multiple registered instances of the same response
void mb_UnregisterListener( const MessageID message, MessageResponse response )
{
	SDL_assert( response != NULL );

	if( message >= sb_Count( sbListeners ) ) {
		return;
	}

	MessageListener* listener = &( sbListeners[message] );
	for( size_t i = 0; i < sb_Count( listener->sbResponses ); ) {
		if( listener->sbResponses[i] == response ) {
			sb_Remove( listener->sbResponses, i );
		} else {
			++i;
		}
	}
}

// unregisters every single listener
void mb_ClearAllListeners( void )
{
	for( size_t i = 0; i < sb_Count( sbListeners ); ++i ) {
		sb_Clear( sbListeners[i].sbResponses );
	}
}

// sends a message out to all the listeners
void mb_BroadcastMessage( const MessageID message, void* data )
{
	if( message >= sb_Count( sbListeners ) ) {
		return;
	}

	MessageListener* listener = &( sbListeners[message] );
	for( size_t i = 0; i < sb_Count( listener->sbResponses ); ++i ) {
		listener->sbResponses[i]( data );
	}
}