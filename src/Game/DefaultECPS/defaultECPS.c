#include <SDL3/SDL_assert.h>
#include <assert.h>

#include "defaultECPS.h"

#include "System\ECPS\entityComponentProcessSystem.h"
#include "System\systems.h"
#include "Graphics\graphics.h"
#include "Utils\helpers.h"
#include "Utils\stretchyBuffer.h"

#define MAX_DEFAULT_COMPONENT_CALLBACKS 32
static int defaultComponentCallbacksCount = 0;
DefaultECPSFunc defaultComponentCallbacks[MAX_DEFAULT_COMPONENT_CALLBACKS];

void registerDefaultECPSComponentCallback( DefaultECPSFunc func )
{
	assert( defaultComponentCallbacksCount < MAX_DEFAULT_COMPONENT_CALLBACKS );
	if( defaultComponentCallbacksCount < MAX_DEFAULT_COMPONENT_CALLBACKS ) {
		defaultComponentCallbacks[defaultComponentCallbacksCount] = func;
		++defaultComponentCallbacksCount;
	}
}

static void callAllComponentCallbacks( void )
{
	for( int i = 0; i < defaultComponentCallbacksCount; ++i ) {
		DefaultECPSFunc func = defaultComponentCallbacks[i];
		if( func ) (*func)( &defaultECPS );
	}
}

#define MAX_DEFAULT_PROCESS_CALLBACKS 32
static int defaultProcessCallbacksCount = 0;
DefaultECPSFunc defaultProcessCallbacks[MAX_DEFAULT_PROCESS_CALLBACKS];

void registerDefaultECPSProcessCallback( DefaultECPSFunc func )
{
	assert( defaultProcessCallbacksCount < MAX_DEFAULT_PROCESS_CALLBACKS );
	if( defaultProcessCallbacksCount < MAX_DEFAULT_PROCESS_CALLBACKS ) {
		defaultProcessCallbacks[defaultProcessCallbacksCount] = func;
		++defaultProcessCallbacksCount;
	}
}


static void callAllProcessCallbacks( void )
{
	for( int i = 0; i < defaultProcessCallbacksCount; ++i ) {
		DefaultECPSFunc func = defaultProcessCallbacks[i];
		if( func ) (*func)( &defaultECPS );
	}
}


ECPS defaultECPS;

typedef struct {
	Process* process;
	int8_t priority;
} PrioritizedProcess;

typedef struct {
	PrioritizedProcess* sbList;
} PriortizedProcessList;

typedef struct {
	void ( *sbEventHandler )( SDL_Event* evt );
	int8_t priority;
} PrioritizedEventHandler;

typedef struct {
	PrioritizedEventHandler* sbList;
} PrioritizedEventHandlerList;

static PriortizedProcessList processProcesses = { NULL };
static PriortizedProcessList drawProcesses = { NULL };
static PriortizedProcessList physicsTickProcesses = { NULL };
static PriortizedProcessList renderProcesses = { NULL };

static PrioritizedEventHandlerList eventHandlers = { NULL };
static PriortizedProcessList clearProcesses = { NULL };

static void addProcessToList( Process* process, int8_t priority, PriortizedProcessList* list )
{
	PrioritizedProcess newProcess = { process, priority };

	// find where to insert it, we'll insert it at the end of the priority it was given
	size_t idx = 0;
	while( ( idx < sb_Count( list->sbList ) ) && ( list->sbList[idx].priority >= newProcess.priority ) ) {
		++idx;
	}

	sb_Insert( list->sbList, idx, newProcess );
}

// these will only hold pointers to the process, they'll have to exist elsewhere to run correctly
void defaultECPS_AddToProcessPhase( Process* process, int8_t priority )
{
	ASSERT_AND_IF_NOT( process != NULL ) return;
	addProcessToList( process, priority, &processProcesses );
}

void defaultECPS_AddToDrawPhase( Process* process, int8_t priority )
{
	ASSERT_AND_IF_NOT( process != NULL ) return;
	addProcessToList( process, priority, &drawProcesses );
}

void defaultECPS_AddToPhysicsTickPhase( Process* process, int8_t priority )
{
	ASSERT_AND_IF_NOT( process != NULL ) return;
	addProcessToList( process, priority, &physicsTickProcesses );
}

void defaultECPS_AddToRenderPhase( Process* process, int8_t priority )
{
	ASSERT_AND_IF_NOT( process != NULL ) return;
	addProcessToList( process, priority, &renderProcesses );
}

void defaultECPS_AddToDrawClearPhase( Process* process, int8_t priority )
{
	ASSERT_AND_IF_NOT( process != NULL ) return;
	addProcessToList( process, priority, &clearProcesses );
}

void defaultECPS_AddEventHandler( void ( *handler )( SDL_Event* ), int8_t priority )
{
	SDL_assert( handler != NULL );

	PrioritizedEventHandler newHandler = { handler, priority };

	// find where to insert it, we'll insert it at the end of the priority it was given
	size_t idx = 0;
	while( ( idx < sb_Count( eventHandlers.sbList ) ) && ( eventHandlers.sbList[idx].priority >= newHandler.priority ) ) {
		++idx;
	}

	sb_Insert( eventHandlers.sbList, idx, newHandler );
}

static void handleEvent( SDL_Event* evt )
{
	// TODO: see if we want to have some sort of fall back set up for this where if an event is handled by one it isn't handled by the others
	for( size_t i = 0; i < sb_Count( eventHandlers.sbList ); ++i ) {
		eventHandlers.sbList[i].sbEventHandler( evt );
	}
}

static void runAllProcesses( PriortizedProcessList* list )
{
	for( size_t i = 0; i < sb_Count( list->sbList ); ++i ) {
		ecps_RunProcess( &defaultECPS, list->sbList[i].process );
	}
}

static void process( void )
{
	runAllProcesses( &processProcesses );
}

static void draw( void )
{
	runAllProcesses( &drawProcesses );
}

static void physicsTick( float dt )
{
	runAllProcesses( &physicsTickProcesses );
}

static void render( float normTimeElapsed )
{
	runAllProcesses( &renderProcesses );
}

static void drawClear( void )
{
	runAllProcesses( &clearProcesses );
}

void defaultECPS_Setup( void )
{
	ecps_StartInitialization( &defaultECPS ); {
		gc_Register( &defaultECPS );
		callAllComponentCallbacks( );

		gp_RegisterProcesses( &defaultECPS );
		callAllProcessCallbacks( );
	} ecps_FinishInitialization( &defaultECPS );

	// set up the initial default process and handler lists
	defaultECPS_AddEventHandler( gp_PointerResponseEventHandler, 0 );

	defaultECPS_AddToProcessPhase( &gpPointerResponseProc, 0 );

	defaultECPS_AddToDrawPhase( &gpDebugDrawPointerReponsesProc, 0 );

	defaultECPS_AddToPhysicsTickPhase( &gpPosTweenProc, 0 );
	defaultECPS_AddToPhysicsTickPhase( &gpScaleTweenProc, 0 );
	defaultECPS_AddToPhysicsTickPhase( &gpAlphaTweenProc, 0 );
	defaultECPS_AddToPhysicsTickPhase( &gpShortLivedProc, 0 );
	defaultECPS_AddToPhysicsTickPhase( &gpAnimSpriteProc, 0 );
	defaultECPS_AddToPhysicsTickPhase( &gpCollisionProc, 0 );

	defaultECPS_AddToRenderPhase( &gp3x3RenderProc, 0 );
	defaultECPS_AddToRenderPhase( &gpRenderProc, 0 );
	defaultECPS_AddToRenderPhase( &gpTextRenderProc, 0 );

	defaultECPS_AddToDrawClearPhase( &gpRenderSwapProc, 0 );

	// set up the functions to be called by the various parts of the engines main loop
	sys_Register( handleEvent, process, draw, physicsTick, render );
	gfx_AddClearCommand( drawClear );
}