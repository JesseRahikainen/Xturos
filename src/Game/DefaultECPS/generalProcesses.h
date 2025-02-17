#ifndef GENERAL_PROCESSES_H
#define GENERAL_PROCESSES_H

#include <SDL3/SDL_events.h>
#include "System/ECPS/ecps_dataTypes.h"
#include "tween.h"
#include "Math/vector2.h"

// general processes shared between games
extern Process gpRenderProc; // these should be set to run in a render command use gfx_AddDrawTrisFunc
extern Process gp3x3RenderProc;
extern Process gpTextRenderProc;

extern Process gpRenderSwapProc; // this should be set to run in a clear command using gfx_AddClearCommand

extern Process gpPointerResponseProc;
extern Process gpCleanUpProc;
extern Process gpDebugDrawPointerReponsesProc;
extern Process gpPosTweenProc;
extern Process gpScaleTweenProc;
extern Process gpAlphaTweenProc;
extern Process gpCollisionProc;
extern Process gpShortLivedProc;
extern Process gpAnimSpriteProc;

void gp_RegisterProcesses( ECPS* ecps );

void gp_PointerResponseEventHandler( SDL_Event * evt );

// helper functions for adding components
void gp_AddPosTween( ECPS* ecps, EntityID entity, float duration, Vector2 startPos, Vector2 endPos, float preDelay, float postDelay, EaseFunc ease );
void gp_AddScaleTween( ECPS* ecps, EntityID entity, float duration, Vector2 startScale, Vector2 endScale, float preDelay, float postDelay, EaseFunc ease );
void gp_AddAlphaTween( ECPS* ecps, EntityID entity, float duration, float startAlpha, float endAlpha, float preDelay, float postDelay, EaseFunc ease );
void gp_AddShortLived( ECPS* ecps, EntityID entity, float duration );

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID tfCompID, ComponentID sprCompID, ComponentID clrCompID, ComponentID floatVal0CompID, ComponentID stencilCompID );
void gp_GeneralSwap( ECPS* epcs, const Entity* entity, ComponentID tfCompID, ComponentID clrCompID, ComponentID floatVal0CompID );

// helper functions for dealing with groups
void gp_AddGroupID( ECPS* ecps, EntityID entity, uint32_t groupID );
void gp_AddGroupIDToEntityAndChildren( ECPS* ecps, EntityID rootEntityID, uint32_t groupID );
void gp_DeleteAllOfGroup( ECPS* ecps, uint32_t groupID );

#endif // inclusion guard