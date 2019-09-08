#include "GeneralProcesses.h"

#include <SDL_assert.h>
#include <math.h>

#include "../Graphics/images.h"
#include "../Input/input.h"
#include "../Utils/stretchyBuffer.h"
#include "../Graphics/camera.h"
#include "../Math/vector2.h"
#include "../Math/vector3.h"
#include "../Math/matrix4.h"
#include "../System/platformLog.h"
#include "../UI/text.h"

#include "../Components/GeneralComponents.h"

// move all of these into separate files?

// ***** Render Process

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID posCompID, ComponentID sprCompID, ComponentID scaleCompID, ComponentID clrCompID, ComponentID rotCompID )
{
	GCPosData* pos = NULL;
	GCSpriteData* sd = NULL;
	GCScaleData* scale = NULL;
	GCColorData* color = NULL;
	GCRotData* rot = NULL;

	ecps_GetComponentFromEntity( entity, posCompID, &pos );
	ecps_GetComponentFromEntity( entity, sprCompID, &sd );

	Vector2 currPos = pos->currPos;
	Vector2 futurePos = pos->futurePos;

	Vector2 currScale = VEC2_ONE;
	Vector2 futureScale = VEC2_ONE;
	if( ecps_GetComponentFromEntity( entity, scaleCompID, &scale ) ) {
		currScale = scale->currScale;
		futureScale = scale->futureScale;

		scale->currScale = futureScale;
	}

	Color currClr = CLR_WHITE;
	Color futureClr = CLR_WHITE;
	if( ecps_GetComponentFromEntity( entity, clrCompID, &color ) ) {
		currClr = color->currClr;
		futureClr = color->futureClr;

		color->currClr = futureClr;
	}

	float currRot = 0.0f;
	float futureRot = 0.0f;
	if( ecps_GetComponentFromEntity( entity, rotCompID, &rot ) ) {
		currRot = rot->currRot;
		futureRot = rot->futureRot;

		rot->currRot = rot->futureRot;
	}

	img_Draw_sv_c_r( sd->img, sd->camFlags, currPos, futurePos, currScale, futureScale, currClr, futureClr, currRot, futureRot, sd->depth );

	pos->currPos = pos->futurePos;
}


Process gpRenderProc;
static void render( ECPS* ecps, const Entity* entity )
{
	gp_GeneralRender( ecps, entity, gcPosCompID, gcSpriteCompID, gcScaleCompID, gcClrCompID, gcRotCompID );
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
	GCRotData* rot = NULL;

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

	img_Draw3x3v_c( sd->imgs, sd->camFlags, currPos, futurePos, currScale, futureScale, currClr, futureClr, sd->depth );

	pos->currPos = pos->futurePos;
}

// ***** Render Text Boxes Process
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

	txt_DisplayTextArea( txt->text, topLeft, currScale, txt->clr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, txt->fontID, 0, NULL, txt->camFlags, txt->depth );
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

static void pointerResponseFinalize_TouchScreen( void )
{
	// TODO: Get this working. Major difference will be the automatic press on focus.
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
			if( prd->overResponse != NULL ) prd->overResponse( &entity );
		}
	}

	if( pointerResponseState == PRS_OVER ) {
		if( currChosenPointerResponseID != prevChosenPointerResponseID ) {
			pointerResponseState = PRS_IDLE;

			if( ecps_GetEntityAndComponentByID( ecps, prevChosenPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->leaveResponse != NULL ) prd->leaveResponse( &entity );
			}
		} else if( pointerResponseMousePressed ) {
			// if the mouse button was pressed, then transition to clicked over state and call the clicked resonse
			focusedPointerResponseID = currChosenPointerResponseID;
			pointerResponseState = PRS_CLICKED_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->pressResponse != NULL ) prd->pressResponse( &entity );
			}
		}
	}

	if( pointerResponseState == PRS_CLICKED_OVER ) {
		if( currChosenPointerResponseID != focusedPointerResponseID ) {
			// mouse was clicked and is no longer over, leave response
			pointerResponseState = PRS_CLICKED_NOT_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->leaveResponse != NULL ) prd->leaveResponse( &entity );
			}
		} else if( pointerResponseMouseReleased ) {
			// mouse released over, call release
			pointerResponseState = PRS_OVER;

			if( ecps_GetEntityAndComponentByID( ecps, focusedPointerResponseID, gcPointerResponseCompID, &entity, &prd ) ) {
				if( prd->releaseResponse != NULL ) prd->releaseResponse( &entity );
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
				if( prd->overResponse != NULL ) prd->overResponse( &entity );
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
#if defined( __ANDROID__ )
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_TouchScreen, &gpPointerResponseProc, 2, gcPosCompID, gcPointerResponseCompID );
#else
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_Mouse, &gpPointerResponseProc, 2, gcPosCompID, gcPointerResponseCompID );
#endif
}