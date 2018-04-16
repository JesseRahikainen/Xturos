#ifndef GENERAL_PROCESSES_H
#define GENERAL_PROCESSES_H

#include "../System/ECPS/ecps_dataTypes.h"

// general processes shared between games
extern Process gpRenderProc;
extern Process gpPointerResponseProc;
extern Process gpCleanUpProc;

void gp_RegisterProcesses( ECPS* ecps );

#endif // inclusion guard