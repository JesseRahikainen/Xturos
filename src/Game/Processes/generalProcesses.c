#include "generalProcesses.h"

#include <SDL_assert.h>
#include <math.h>

#include "Graphics/images.h"
#include "Input/input.h"
#include "Utils/stretchyBuffer.h"
#include "Graphics/camera.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Math/matrix4.h"
#include "System/platformLog.h"
#include "UI/text.h"

#include "Components/generalComponents.h"

// move all of these into separate files?

// ***** Render Process

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID posCompID, ComponentID sprCompID, ComponentID scaleCompID, ComponentID clrCompID, ComponentID rotCompID, ComponentID floatVal0CompID, ComponentID stencilCompID )
{
	GCPosData* pos = NULL;
	GCSpriteData* sd = NULL;
	GCScaleData* scale = NULL;
	GCColorData* color = NULL;
	GCRotData* rot = NULL;
	GCFloatVal0Data* floatVal0 = NULL;
	GCStencilData* stencil = NULL;

	ecps_GetComponentFromEntity( entity, posCompID, &pos );
	ecps_GetComponentFromEntity( entity, sprCompID, &sd );

	int drawID = img_CreateDraw( sd->img, sd->camFlags, pos->currPos, pos->futurePos, sd->depth );
	pos->currPos = pos->futurePos;

	if( ecps_GetComponentFromEntity( entity, scaleCompID, &scale ) ) {
		img_SetDrawScaleV( drawID, scale->currScale, scale->futureScale );
		scale->currScale = scale->futureScale;
	}

	if( ecps_GetComponentFromEntity( entity, clrCompID, &color ) ) {
		img_SetDrawColor( drawID, color->currClr, color->futureClr );
		color->currClr = color->futureClr;
	}

	if( ecps_GetComponentFromEntity( entity, rotCompID, &rot ) ) {
		img_SetDrawRotation( drawID, rot->currRot, rot->futureRot );
		rot->currRot = rot->futureRot;
	}

	if( ecps_GetComponentFromEntity( entity, floatVal0CompID, &floatVal0 ) ) {
		img_SetDrawFloatVal0( drawID, floatVal0->currValue, floatVal0->futureValue );
		floatVal0->currValue = floatVal0->futureValue;
	}

	if( ecps_GetComponentFromEntity( entity, stencilCompID, &stencil ) ) {
		img_SetDrawStencil( drawID, stencil->isStencil, stencil->stencilID );
	}
}


Process gpRenderProc;
static void render( ECPS* ecps, const Entity* entity )
{
	gp_GeneralRender( ecps, entity, gcPosCompID, gcSpriteCompID, gcScaleCompID, gcClrCompID, gcRotCompID, gcFloatVal0CompID, gcStencilCompID );
}

// ***** Render 3x3 Process
//  Is there a way to wrap this up into the general render process?
Process gp3x3RenderProc;
static void render3x3( ECPS* ecps, const Entity* entity )
{
	GCPosData* pos = NULL;
	GC3x3SpriteData* sd = NULL;
	GCScaleData* scale = NULL;
	GCColorData* color = NULL;
	GCFloatVal0Data* floatVal = NULL;

	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );
	ecps_GetComponentFromEntity( entity, gc3x3SpriteCompID, &sd );

	Vector2 currPos = pos->currPos;
	Vector2 futurePos = pos->futurePos;

	Vector2 currScale = VEC2_ONE;
	Vector2 futureScale = VEC2_ONE;
	ecps_GetComponentFromEntity( entity, gcScaleCompID, &scale );
	currScale = scale->currScale;
	futureScale = scale->futureScale;

	scale->currScale = futureScale;

	Color currClr = CLR_WHITE;
	Color futureClr = CLR_WHITE;
	if( ecps_GetComponentFromEntity( entity, gcClrCompID, &color ) ) {
		currClr = color->currClr;
		futureClr = color->futureClr;

		color->currClr = futureClr;
	}

	float currVal0 = 0.0f;
	float futureVal0 = 0.0f;
	if( ecps_GetComponentFromEntity( entity, gcFloatVal0CompID, &floatVal ) ) {
		currVal0 = floatVal->currValue;
		futureVal0 = floatVal->futureValue;
	}

	img_Draw3x3v_c_f( sd->imgs, sd->camFlags, currPos, futurePos, currScale, futureScale, currClr, futureClr, currVal0, futureVal0, sd->depth );

	pos->currPos = pos->futurePos;
}

// ***** Render Text Boxes Process
// TODO: Get this working better
Process gpTextRenderProc;
static void renderText( ECPS* ecps, const Entity* entity )
{
	GCTextData* txt = NULL;
	GCPosData* pos = NULL; // pos is centered
	GCScaleData* scale = NULL; // scale is used as size

	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );
	ecps_GetComponentFromEntity( entity, gcTextCompID, &txt );
	ecps_GetComponentFromEntity( entity, gcScaleCompID, &scale );

	Vector2 currPos = pos->currPos;

	Vector2 currScale = scale->currScale;

	Vector2 topLeft;
	vec2_AddScaled( &currPos, &currScale, -0.5f, &topLeft );

	txt_DisplayTextArea( txt->text, topLeft, currScale, txt->clr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, txt->fontID, 0, NULL, txt->camFlags, txt->depth, (float)txt_GetBaseSize( txt->fontID ) );
}

// ***** Clickable Objects
#define PR_IDLE = 0;
#define PR_OVER = 1;
#define PR_CLICKED = 2;

Process gpPointerResponseProc;

static bool pointerResponseMousePressed = false;
static bool pointerResponseMouseReleased = false;
static bool pointerResponseMouseValid;

typedef struct {
	Vector3 adjPos;
	uint32_t camFlags;
} TransformedResponseMousePos;
TransformedResponseMousePos* sbResponseMousePos = NULL;

typedef enum {
	PRS_IDLE,
	PRS_OVER,
	PRS_CLICKED_OVER,
	PRS_CLICKED_NOT_OVER
} PointerResponseState;

static EntityID focusedPointerResponseID = INVALID_ENTITY_ID;
static PointerResponseState pointerResponseState = PRS_IDLE;

static EntityID currChosenPointerResponseID = INVALID_ENTITY_ID;
static int32_t currChosenPriority = INT32_MIN;

static EntityID prevChosenPointerResponseID = INVALID_ENTITY_ID;

static void pointerResponseGetMouse( ECPS* ecps )
{
	// need to get the mouse position, as well as seeing if we need to do any more processing for the mouse
	Vector2 mouse2DPos;
	Vector3 buttonMousePos;
	pointerResponseMouseValid = input_GetMousePosition( &mouse2DPos );
	if( !pointerResponseMouseValid ) return;
	vec2ToVec3( &mouse2DPos, 0.0f, &buttonMousePos );
	
	sb_Clear( sbResponseMousePos );

	// generate the transformed mouse positions based on each active camera
	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		TransformedResponseMousePos tfPos;

		tfPos.camFlags = cam_GetFlags( currCamera );
		Matrix4 camMat;
		cam_GetInverseViewMatrix( currCamera, &camMat );
		mat4_TransformVec3Pos( &camMat, &buttonMousePos, &( tfPos.adjPos ) );

		sb_Push( sbResponseMousePos, tfPos );
	}

	currChosenPointerResponseID = INVALID_ENTITY_ID;
	currChosenPriority = INT32_MIN;
}

static void pointerResponseDetectState( ECPS* ecps, const Entity* entity )
{
	if( !pointerResponseMouseValid ) return;

	GCPointerResponseData* responseData = NULL;
	ecps_GetComponentFromEntity( entity, gcPointerResponseCompID, &responseData );
	if( responseData->priority < currChosenPriority ) {
		return;
	}

	GCPosData* posData = NULL;
	ecps_GetComponentFromEntity( entity, gcPosCompID, &posData );
	//PointerResponseState prevState;

	Vector3 diff;
	for( size_t i = 0; i < sb_Count( sbResponseMousePos ); ++i ) {
		if( !( responseData->camFlags & sbResponseMousePos[i].camFlags ) ) continue;

		// see if any of the local mouse pointer is in this entity
		//prevState = responseData->state;
		diff.x = posData->currPos.x - sbResponseMousePos[i].adjPos.x;
		diff.y = posData->currPos.y - sbResponseMousePos[i].adjPos.y;
		if( ( fabsf( diff.x ) <= responseData->collisionHalfDim.x ) && ( fabsf( diff.y ) <= responseData->collisionHalfDim.y ) ) {
			currChosenPriority = responseData->priority;
			currChosenPointerResponseID = entity->id;
		}
	}
}

static void pointerResponseFinalize_TouchScreen( ECPS* ecps )
{
	/*
	Mobile pointer buttons will act slightly differently since you're either touching
	the screen or not, there is no pointer to hover over the button. We have no over
	state.

	       +----4----+   +------3------+
	       V         |   V             |
	      idle -0-> clicked -1-> clickedNotOver
		   ^                           |
		   +------------2--------------+

		  0 - pressed button
		  1 - move before releasing button
		  2 - touch released
		  3 - moved finger over button
		  4 - touch released
	*/

	Entity entity;
	GCPointerResponseData* prd = NULL;
	if( ( pointerResponseState == PRS_IDLE ) && ( currChosenPointerResponseID != INVALID_ENTITY_ID ) && pointerResponseMousePressed ) {
		// touch is currently over chosen entity, switch from idle to over, call over and press response
		pointerResponseState = PRS_CLICKED_OVER;
		focusedPointerResponseID = currChosenPointerResponseID;

		if( ecps_GetEntityAndComponentByID( ecps, currChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
			if( prd->overResponse != NULL ) {
				prd->overResponse( &entity );
			}
			if( prd->pressResponse != NULL ) {
				prd->pressResponse( &entity );
			}
		}
	}

	if( pointerResponseState == PRS_CLICKED_OVER ) {
		if( currChosenPointerResponseID != focusedPointerResponseID ) {
			// finger was down and is no longer over, leave response
			pointerResponseState = PRS_CLICKED_NOT_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->leaveResponse != NULL ) {
					prd->leaveResponse( &entity );
				}
			}
		} else if( pointerResponseMouseReleased ) {
			// finger released over, call release
			pointerResponseState = PRS_IDLE;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->releaseResponse != NULL ) {
					prd->releaseResponse( &entity );
				}
			}
		}
	}

	if( pointerResponseState == PRS_CLICKED_NOT_OVER ) {
		if( pointerResponseMouseReleased ) {
			// finger was released, but not over focused button, no response
			pointerResponseState = PRS_IDLE;
			currChosenPointerResponseID = INVALID_ENTITY_ID;

		} else if( pointerResponseState == currChosenPriority ) {
			// finger moved back over button, over response, press response? create a secondary press response for just graphics?
			//  or just don't do over, once you're here you're stuck
			pointerResponseState = PRS_CLICKED_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, currChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->overResponse != NULL ) {
					prd->overResponse( &entity );
				}
			}
		}
	}

	prevChosenPointerResponseID = currChosenPointerResponseID;
	pointerResponseMousePressed = false;
	pointerResponseMouseReleased = false;
}

static void pointerResponseFinalize_Mouse( ECPS* ecps )
{
	/*
	We have hover and clicked on as two separate state machines.
	If you've clicked the response entity is locked on the one you're over.
	Release gets whether you are over or not.
	This state machine is for the Process, not for individual buttons

				LEAVE		RELEASE		  OVER
			   +---6---+  +--4----+  +------5------+
			   V	   |  V		  |  V             |
	         idle -0-> over -1-> clicked -2-> clickedNotOver
			   ^  OVER		PRESS		LEAVE	   |
			   +-----------------3-----------------+
								NONE

			   0 - mouse moved over button (Over)
			   1 - mouse pressed, this button becomes the focused button, no other buttons will run (Press)
			   2 - mouse moved off button (Leave)
			   3 - mouse released (none)
			   4 - mouse released (Release)
			   5 - mouse moved over button (Over)
			   6 - mouse moved off button (Leave)
	*/

	Entity entity;
	GCPointerResponseData* prd = NULL;

	if( ( pointerResponseState == PRS_IDLE ) && ( currChosenPointerResponseID != INVALID_ENTITY_ID ) ) {
		// mouse is currently over chosen entity, switch from idle to over, call over response
		pointerResponseState = PRS_OVER;

		if( ecps_GetEntityAndComponentByID( ecps, currChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
			if( prd->overResponse != NULL ) {
				prd->overResponse( &entity );
			}
		}
	}

	if( pointerResponseState == PRS_OVER ) {
		if( currChosenPointerResponseID != prevChosenPointerResponseID ) {
			pointerResponseState = PRS_IDLE;

			if( ecps_GetEntityAndComponentByID( ecps, prevChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->leaveResponse != NULL ) {
					prd->leaveResponse( &entity );
				}
			}
		} else if( pointerResponseMousePressed ) {
			// if the mouse button was pressed, then transition to clicked over state and call the clicked resonse
			focusedPointerResponseID = currChosenPointerResponseID;
			pointerResponseState = PRS_CLICKED_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->pressResponse != NULL ) {
					prd->pressResponse( &entity );
				}
			}
		}
	}

	if( pointerResponseState == PRS_CLICKED_OVER ) {
		if( currChosenPointerResponseID != focusedPointerResponseID ) {
			// mouse was clicked and is no longer over, leave response
			pointerResponseState = PRS_CLICKED_NOT_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->leaveResponse != NULL ) {
					prd->leaveResponse( &entity );
				}
			}
		} else if( pointerResponseMouseReleased ) {
			// mouse released over, call release
			pointerResponseState = PRS_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->releaseResponse != NULL ) {
					prd->releaseResponse( &entity );
				}
			}
		}
	}

	if( pointerResponseState == PRS_CLICKED_NOT_OVER ) {
		if( pointerResponseMouseReleased ) {
			// mouse released, but not over focused button, no response
			pointerResponseState = PRS_IDLE;

		} else if( pointerResponseState == currChosenPriority ) {
			// mouse moved back over button, over response, press response? create a secondary press response for just graphics?
			//  or just don't do over, once you're here you're stuck
			pointerResponseState = PRS_CLICKED_OVER;
			
			if( ecps_GetEntityAndComponentByID( ecps, currChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->overResponse != NULL ) {
					prd->overResponse( &entity );
				}
			}
		}
	}

	prevChosenPointerResponseID = currChosenPointerResponseID;
	pointerResponseMousePressed = false;
	pointerResponseMouseReleased = false;
}

// needed to handle button presses
void gp_PointerResponseEventHandler( SDL_Event * evt )
{
	if( evt->type == SDL_MOUSEBUTTONDOWN ) {
		if( evt->button.button == SDL_BUTTON_LEFT ) {
			pointerResponseMousePressed = true;
		}
	} else if( evt->type == SDL_MOUSEBUTTONUP ) {
		if( evt->button.button == SDL_BUTTON_LEFT ) {
			pointerResponseMouseReleased = true;
		}
	}
}

// ***** Clean Up
Process gpCleanUpProc;
static void cleanUp( ECPS* ecps, Entity* entity )
{
	llog( LOG_VERBOSE, "Destroying entity: %X08", entity->id );
	ecps_DestroyEntity( ecps, entity );
}

// ***** Mounted Position Update
//  this should be run after everything that updates the position, but before anything that uses the position
Process gpUpdateMountedPosProc;
static void updateMounted( ECPS* ecps, const Entity* entity )
{
	GCMountedPosOffset* offset = NULL;
	GCPosData* pos = NULL;

	ecps_GetComponentFromEntity( entity, gcMountedPosOffsetCompID, &offset );
	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );

	GCPosData* parentPos = NULL;
	if( !ecps_GetComponentFromEntityByID( ecps, offset->parentID, gcPosCompID, &parentPos ) ) {
		return;
	}

	vec2_Add( &( parentPos->futurePos ), &( offset->offset ), &( pos->futurePos ) );
}

// ***** Mounted Position Update
Process gpClampMountedPosProc;
static void mountedClamp( ECPS* ecps, const Entity* entity )
{
	GCMountedPosOffset* offset = NULL;
	GCPosData* pos = NULL;

	ecps_GetComponentFromEntity( entity, gcMountedPosOffsetCompID, &offset );
	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );

	GCPosData* parentPos = NULL;
	if( !ecps_GetComponentFromEntityByID( ecps, offset->parentID, gcPosCompID, &parentPos ) ) {
		return;
	}

	vec2_Add( &( parentPos->futurePos ), &( offset->offset ), &( pos->futurePos ) );
	vec2_Add( &( parentPos->currPos ), &( offset->offset ), &( pos->currPos ) );
}

// ***** Draw PointerResponses for debugging
Process gpDebugDrawPointerReponsesProc;
#include "Graphics/debugRendering.h"
static void drawPointerResponse( ECPS* ecps, const Entity* entity )
{
	GCPointerResponseData* pointer = NULL;
	GCPosData* pos = NULL;

	ecps_GetComponentFromEntity( entity, gcPointerResponseCompID, &pointer );
	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );

	Vector2 topLeft;
	Vector2 fullSize;
	vec2_Subtract( &pos->currPos, &pointer->collisionHalfDim, &topLeft );
	vec2_Scale( &pointer->collisionHalfDim, 2.0f, &fullSize );
	debugRenderer_AABB( pointer->camFlags, topLeft, fullSize, CLR_BLUE );
}

// ***** General tweening helper methods
#include "System/gameTime.h"
static bool runVec2Tween( GCVec2TweenData* tween, Vector2* out )
{
	tween->timePassed += PHYSICS_DT;
	float t = tween->timePassed / tween->duration;
	t = MIN( 1.0f, t );

	float adjT = inverseLerp( tween->preDelay, tween->postDelay, t );
	if( tween->easing ) {
		adjT = tween->easing( adjT );
	}
	vec2_Lerp( &( tween->startState ), &( tween->endState ), adjT, out );

	return ( tween->timePassed >= tween->duration );
}

static bool runFloatTween( GCFloatTweenData* tween, float* out )
{
	tween->timePassed += PHYSICS_DT;
	float t = tween->timePassed / tween->duration;
	t = MIN( 1.0f, t );

	float adjT = inverseLerp( tween->preDelay, tween->postDelay, t );
	if( tween->easing ) {
		adjT = tween->easing( adjT );
	}
	(*out) = lerp( tween->startState, tween->endState, adjT );

	return ( tween->timePassed >= tween->duration );
}

// ***** Position tweening
Process gpPosTweenProc;
static void posTweenUpdate( ECPS* ecps, const Entity* entity )
{
	GCPosData* pos = NULL;
	GCVec2TweenData* posTween = NULL;

	ecps_GetComponentFromEntity( entity, gcPosCompID, &pos );
	ecps_GetComponentFromEntity( entity, gcPosTweenCompID, &posTween );

	if( runVec2Tween( posTween, &( pos->futurePos ) ) ) {
		// done, remove
		ecps_RemoveComponentFromEntityMidProcess( ecps, entity, gcPosTweenCompID );
	}
}

void gp_AddPosTween( ECPS* ecps, EntityID entity, float duration, Vector2 startPos, Vector2 endPos, float preDelay, float postDelay, EaseFunc ease )
{
	GCVec2TweenData tween;
	tween.duration = duration;
	tween.timePassed = 0.0f;
	tween.startState = startPos;
	tween.endState = endPos;
	tween.preDelay = preDelay;
	tween.postDelay = postDelay;
	tween.easing = ease;
	ecps_AddComponentToEntityByID( ecps, entity, gcPosTweenCompID, &tween );
}

// ***** Scale tweening
Process gpScaleTweenProc;
static void scaleTweenUpdate( ECPS* ecps, const Entity* entity )
{
	GCScaleData* scale = NULL;
	GCVec2TweenData* scaleTween = NULL;

	ecps_GetComponentFromEntity( entity, gcScaleCompID, &scale );
	ecps_GetComponentFromEntity( entity, gcScaleTweenCompID, &scaleTween );

	if( runVec2Tween( scaleTween, &( scale->futureScale ) ) ) {
		// done, remove
		ecps_RemoveComponentFromEntityMidProcess( ecps, entity, gcScaleTweenCompID );
	}
}

void gp_AddScaleTween( ECPS* ecps, EntityID entity, float duration, Vector2 startScale, Vector2 endScale, float preDelay, float postDelay, EaseFunc ease )
{
	GCVec2TweenData tween;
	tween.duration = duration;
	tween.timePassed = 0.0f;
	tween.startState = startScale;
	tween.endState = endScale;
	tween.preDelay = preDelay;
	tween.postDelay = postDelay;
	tween.easing = ease;
	ecps_AddComponentToEntityByID( ecps, entity, gcScaleTweenCompID, &tween );
}

// ***** Alpha tweening
Process gpAlphaTweenProc;
static void alphaTweenUpdate( ECPS* ecps, const Entity* entity )
{
	GCColorData* clr = NULL;
	GCFloatTweenData* alphaTween = NULL;

	ecps_GetComponentFromEntity( entity, gcClrCompID, &clr );
	ecps_GetComponentFromEntity( entity, gcAlphaTweenCompID, &alphaTween );

	if( runFloatTween( alphaTween, &( clr->futureClr.a ) ) ) {
		// done, remove
		ecps_RemoveComponentFromEntityMidProcess( ecps, entity, gcAlphaTweenCompID );
	}
}

void gp_AddAlphaTween( ECPS* ecps, EntityID entity, float duration, float startAlpha, float endAlpha, float preDelay, float postDelay, EaseFunc ease )
{
	GCFloatTweenData tween;
	tween.duration = duration;
	tween.timePassed = 0.0f;
	tween.startState = startAlpha;
	tween.endState = endAlpha;
	tween.preDelay = preDelay;
	tween.postDelay = postDelay;
	tween.easing = ease;
	ecps_AddComponentToEntityByID( ecps, entity, gcAlphaTweenCompID, &tween );

	// set the base color
	GCColorData* clrData = NULL;
	if( ecps_GetComponentFromEntityByID( ecps, entity, gcClrCompID, &clrData ) ) {
		clrData->currClr.a = clrData->futureClr.a = startAlpha;
	}
}

// ***** Register the processes
void gp_RegisterProcesses( ECPS* ecps )
{
	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcSpriteCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "DRAW", NULL, render, NULL, &gpRenderProc, 2, gcPosCompID, gcSpriteCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gc3x3SpriteCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcScaleCompID != INVALID_COMPONENT_ID ); // TODO?: create a separate component for size instead of scale
	ecps_CreateProcess( ecps, "3x3", NULL, render3x3, NULL, &gp3x3RenderProc, 3, gcPosCompID, gc3x3SpriteCompID, gcScaleCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcTextCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcScaleCompID != INVALID_COMPONENT_ID ); // TODO?: create a separate component for size instead of scale
	ecps_CreateProcess( ecps, "TEXT", NULL, renderText, NULL, &gpTextRenderProc, gcPosCompID, gcTextCompID, gcScaleCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPointerResponseCompID != INVALID_COMPONENT_ID );
/*#if defined( __ANDROID__ )
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_TouchScreen, &gpPointerResponseProc, 2, gcPosCompID, gcPointerResponseCompID );
#else
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_Mouse, &gpPointerResponseProc, 2, gcPosCompID, gcPointerResponseCompID );
#endif//*/
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_TouchScreen, &gpPointerResponseProc, 2, gcPosCompID, gcPointerResponseCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcMountedPosOffsetCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "MOUNTED_UPDATE", NULL, updateMounted, NULL, &gpUpdateMountedPosProc, 2, gcPosCompID, gcMountedPosOffsetCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcMountedPosOffsetCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "MOUNTED_CLAMP", NULL, mountedClamp, NULL, &gpClampMountedPosProc, 2, gcPosCompID, gcMountedPosOffsetCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPointerResponseCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "CLK_DBG", NULL, drawPointerResponse, NULL, &gpDebugDrawPointerReponsesProc, 2, gcPosCompID, gcPointerResponseCompID );

	SDL_assert( gcPosCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPosTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "POS_TWEEN", NULL, posTweenUpdate, NULL, &gpPosTweenProc, 2, gcPosCompID, gcPosTweenCompID );

	SDL_assert( gcScaleCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcScaleTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "SCALE_TWEEN", NULL, scaleTweenUpdate, NULL, &gpScaleTweenProc, 2, gcScaleCompID, gcScaleTweenCompID );

	SDL_assert( gcClrCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcAlphaTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "ALPHA_TWEEN", NULL, alphaTweenUpdate, NULL, &gpAlphaTweenProc, 2, gcClrCompID, gcAlphaTweenCompID );
}
