#ifndef GENERAL_PROCESSES_H
#define GENERAL_PROCESSES_H

#include <SDL_events.h>
#include "../System/ECPS/ecps_dataTypes.h"

// general processes shared between games
extern Process gpRenderProc;
extern Process gp3x3RenderProc;
extern Process gpTextRenderProc;
extern Process gpPointerResponseProc;
extern Process gpCleanUpProc;

void gp_RegisterProcesses( ECPS* ecps );

void gp_PointerResponseEventHandler( SDL_Event * evt );

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID posCompID, ComponentID sprCompID, ComponentID scaleCompID, ComponentID clrCompID, ComponentID rotCompID );

#endif // inclusion guard