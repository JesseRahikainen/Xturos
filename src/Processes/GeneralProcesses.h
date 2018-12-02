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

#endif // inclusion guard