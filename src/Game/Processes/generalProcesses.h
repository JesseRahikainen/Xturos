#ifndef GENERAL_PROCESSES_H
#define GENERAL_PROCESSES_H

#include <SDL_events.h>
#include "System/ECPS/ecps_dataTypes.h"
#include "tween.h"
#include "Math/vector2.h"

// general processes shared between games
extern Process gpRenderProc;
extern Process gp3x3RenderProc;
extern Process gpTextRenderProc;
extern Process gpPointerResponseProc;
extern Process gpCleanUpProc;
extern Process gpUpdateMountedPosProc;
extern Process gpClampMountedPosProc;
extern Process gpDebugDrawPointerReponsesProc;
extern Process gpPosTweenProc;
extern Process gpScaleTweenProc;
extern Process gpAlphaTweenProc;

void gp_RegisterProcesses( ECPS* ecps );

void gp_PointerResponseEventHandler( SDL_Event * evt );

// helper functions for adding tweens
void gp_AddPosTween( ECPS* ecps, EntityID entity, float duration, Vector2 startPos, Vector2 endPos, float preDelay, float postDelay, EaseFunc ease );
void gp_AddScaleTween( ECPS* ecps, EntityID entity, float duration, Vector2 startScale, Vector2 endScale, float preDelay, float postDelay, EaseFunc ease );
void gp_AddAlphaTween( ECPS* ecps, EntityID entity, float duration, float startAlpha, float endAlpha, float preDelay, float postDelay, EaseFunc ease );

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID posCompID, ComponentID sprCompID, ComponentID scaleCompID, ComponentID clrCompID, ComponentID rotCompID, ComponentID floatVal0CompID, ComponentID stencilCompID );

#endif // inclusion guard