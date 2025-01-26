#ifndef GAME_PROCESSES_H
#define GAME_PROCESSES_H

#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "System/ECPS/ecps_dataTypes.h"

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

// register functions to be called during defaultECPS_Setup(), need component and process registration functions
typedef void (*DefaultECPSFunc)( ECPS* );

#if defined( _MSC_VER )

#pragma section("regcomp$a", read) // beginning
#pragma section("regcomp$m", read) // storage
#pragma section("regcomp$z", read) // end
#define ADD_DEFAULT_COMPONENT_REGISTER __declspec( allocate( "regcomp$m" ) ) 
__declspec( allocate( "regcomp$a" ) ) static const DefaultECPSFunc startRegisterComponentsFuncs = NULL;
__declspec( allocate( "regcomp$z" ) ) static const DefaultECPSFunc endRegisterComponentsFuncs = NULL;

#pragma section("regproc$a", read) // beginning
#pragma section("regproc$m", read) // storage
#pragma section("regproc$z", read) // end
#define ADD_DEFAULT_PROCESS_REGISTER __declspec( allocate( "regproc$m" ) ) 
__declspec( allocate( "regproc$a" ) ) static const DefaultECPSFunc startRegisterProcessesFuncs = NULL;
__declspec( allocate( "regproc$z" ) ) static const DefaultECPSFunc endRegisterProcessFuncs = NULL;

#elif defined( __EMSCRIPTEN__ )
#include <emscripten.h>
#define ADD_DEFAULT_COMPONENT_REGISTER __attribute__( ( used, retain, section( "regcomp" ) ) )
#define ADD_DEFAULT_PROCESS_REGISTER __attribute__( ( used, retain, section( "regproc" ) ) )
#else
#error Default process callbacks not set up for this platform.
#endif

#define ADD_DEFAULT_ECPS_COMPONENT_CALLBACK(fn) \
	ADD_DEFAULT_COMPONENT_REGISTER const DefaultECPSFunc defaultCompECPSFunction##fn = fn;

#define ADD_DEFAULT_ECPS_PROCESS_CALLBACK(fn) \
	ADD_DEFAULT_PROCESS_REGISTER const DefaultECPSFunc defaultProcECPSFunction##fn = fn;

#endif
