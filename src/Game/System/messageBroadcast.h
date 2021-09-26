#ifndef MESSAGE_BROADCAST_H
#define MESSAGE_BROADCAST_H

#include <stdlib.h>

typedef void (*MessageResponse)(void*);
typedef size_t MessageID;

// register a message listener, you can register the same listener more than once
void mb_RegisterListener( const MessageID message, MessageResponse response );

// unregisters the listener for the message, will unregister all if there are multiple registered instances of the same response
void mb_UnregisterListener( const MessageID message, MessageResponse response );

// unregisters every single listener
void mb_ClearAllListeners( void );

// sends a message out to all the listeners
void mb_BroadcastMessage( const MessageID message, void* data );

#endif