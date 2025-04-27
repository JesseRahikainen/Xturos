#ifndef GAME_PROCESSES_H
#define GAME_PROCESSES_H

#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "System/ECPS/ecps_dataTypes.h"
#include "Utils/helpers.h"

// we'll have the default components, the default processes, the game specific processes, the game specific processes, the system created to run all the processes, the additional processes for each
//  phase of the gameloop
// most of the game specific processes can start as custom processes that we can register later
// game specific components will have to be registered and are not as flexible as the processes
// so how do we divide this then? we have the defaultECPS along with it's components and processes
// 
// we just need defaultECPS_*
// defaultECPS.c/h files, calling the default registration will call the game specific registration like it currently will
// will have a defaultECPS of the type ECPS, that is globally accessible like globalFSM.
// internally this will use a system that will go through a list of processes and run them at certain points of the game loop
extern ECPS defaultECPS;

void defaultECPS_AddToProcessPhase( Process* process, int8_t priority );
void defaultECPS_AddToDrawPhase( Process* process, int8_t priority );
void defaultECPS_AddToPhysicsTickPhase( Process* process, int8_t priority );
void defaultECPS_AddToRenderPhase( Process* process, int8_t priority );
void defaultECPS_AddToDrawClearPhase( Process* process, int8_t priority );
void defaultECPS_AddEventHandler( void ( *handler )( SDL_Event* ), int8_t priority );

void defaultECPS_Setup( void );

typedef void (*DefaultECPSFunc)( ECPS* );

void registerDefaultECPSComponentCallback( DefaultECPSFunc func );
// creates a function that is called before main() that will add this function to the list of default ECPS component functions
#define ADD_DEFAULT_ECPS_COMPONENT_CALLBACK(fn) INITIALIZER( register_fn##ComponentFunction_ ) { registerDefaultECPSComponentCallback( fn ); }

void registerDefaultECPSProcessCallback( DefaultECPSFunc func );
// creates a function that is called before main() that will add this function to the list of default ECPS process functions
#define ADD_DEFAULT_ECPS_PROCESS_CALLBACK(fn) INITIALIZER( register_fn##ProcessFunction_ ) { registerDefaultECPSProcessCallback( fn ); }

#endif
