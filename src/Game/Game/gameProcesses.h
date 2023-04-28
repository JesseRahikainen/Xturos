#ifndef GAME_PROCESSES_H
#define GAME_PROCESSES_H

#include "System/ECPS/ecps_dataTypes.h"

// Add game specific processes here.
extern Process followMouseProc;

void gameProcesses_Setup( ECPS* ecps );

void gameProcesses_RunDroppableTargets( ECPS* ecps, Entity* droppedEntity );

#endif
