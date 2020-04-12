#ifndef SYSTEMS_H
#define SYSTEMS_H

#include <SDL_events.h>
#include <inttypes.h>

typedef void (*SystemProcessEventsFunc)( SDL_Event* e );
typedef void (*SystemProcessFunc)( void );
typedef void (*SystemDrawFunc)( void );
typedef void (*SystemPhysicsTickFunc)( float dt );

// TODO: Add a priority to a system, systems with higher priority are always processed before systems with a lower priority.

/*
Registers the associated functions into a system.
 Returns the ID used for the system, which is used to unregister it later.
 If something goes wrong it returns an ID that's < 0.
*/
int sys_Register( SystemProcessEventsFunc procEvents, SystemProcessFunc proc, SystemDrawFunc draw, SystemPhysicsTickFunc tick );

/*
Unregisters the associated system.
*/
void sys_UnRegister( int systemID );

// Runs the matching function on all the registered systems.
void sys_ProcessEvents( SDL_Event* e );
void sys_Process( void );
void sys_Draw( void );
void sys_PhysicsTick( float dt );

#endif /* inclusion guard */