#include "systems.h"

#include "platformLog.h"

typedef struct {
	SystemProcessEventsFunc procEvents;
	SystemProcessFunc proc;
	SystemDrawFunc draw;
	SystemPhysicsTickFunc tick;
} System;

static System emptySystem = { NULL, NULL, NULL, NULL };

#define MAX_SYSTEMS 32

static System systems[MAX_SYSTEMS];

int systemInUse( int idx )
{
	return ( ( systems[idx].procEvents != NULL ) ||
			 ( systems[idx].proc != NULL ) ||
			 ( systems[idx].draw != NULL ) ||
			 ( systems[idx].tick != NULL ) );
}

int sys_Register( SystemProcessEventsFunc procEvents, SystemProcessFunc proc, SystemDrawFunc draw, SystemPhysicsTickFunc tick )
{
	int idx = 0;
	while( ( idx < MAX_SYSTEMS ) && systemInUse( idx ) ) {
		++idx;
	}

	if( idx >= MAX_SYSTEMS ) {
		return -1;
	}

	systems[idx].procEvents = procEvents;
	systems[idx].proc = proc;
	systems[idx].draw = draw;
	systems[idx].tick = tick;

	return idx;
}

void sys_UnRegister( int systemID )
{
	if( ( systemID < 0 ) || ( systemID > MAX_SYSTEMS ) ) {
		llog( LOG_DEBUG, "Attempting to unregister an invalid system" );
		return;
	}

	if( !systemInUse( systemID ) ) {
		llog( LOG_DEBUG, "Attempting to unregister a system that has not been registered" );
		return;
	}

	systems[systemID] = emptySystem;
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