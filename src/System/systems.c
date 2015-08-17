#include "systems.h"

#include <SDL_log.h>

typedef struct {
	SystemProcessEventsFunc procEvents;
	SystemProcessFunc proc;
	SystemDrawFunc draw;
	SystemPhysicsTickFunc tick;
} System;

static System emptySystem = { NULL, NULL, NULL, NULL };

#define MAX_SYSTEMS 32

static System systems[MAX_SYSTEMS];
static int lastSystem = -1;

int sys_Register( SystemProcessEventsFunc procEvents, SystemProcessFunc proc, SystemDrawFunc draw, SystemPhysicsTickFunc tick )
{
	if( lastSystem >= ( MAX_SYSTEMS - 1 ) ) {
		return -1;
	}

	++lastSystem;
	systems[lastSystem].procEvents = procEvents;
	systems[lastSystem].proc = proc;
	systems[lastSystem].draw = draw;
	systems[lastSystem].tick = tick;

	return lastSystem;
}

void sys_UnRegister( int systemID )
{
	if( ( systemID < 0 ) || ( systemID > lastSystem ) ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, "Attempting to unregister an invalid system" );
		return;
	}

	int idx;
	for( idx = systemID; idx < lastSystem; ++idx ) {
		systems[idx] = systems[idx+1];
	}
	--lastSystem;
}

// Runs the matching function on all the registered systems.
void sys_ProcessEvents( SDL_Event* e )
{
	for( int i = 0; i < MAX_SYSTEMS; ++i ) {
		if( systems[i].procEvents != NULL ) {
			( *( systems[i].procEvents ) )( e );
		}
	}
}

void sys_Process( void )
{
	for( int i = 0; i < MAX_SYSTEMS; ++i ) {
		if( systems[i].proc != NULL ) {
			( *( systems[i].proc ) )( );
		}
	}
}

void sys_Draw( void )
{
	for( int i = 0; i < MAX_SYSTEMS; ++i ) {
		if( systems[i].draw != NULL ) {
			( *( systems[i].draw) )( );
		}
	}
}

void sys_PhysicsTick( float dt )
{
	for( int i = 0; i < MAX_SYSTEMS; ++i ) {
		if( systems[i].tick != NULL ) {
			( *( systems[i].tick ) )( dt );
		}
	}
}