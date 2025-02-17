#include "generalProcesses.h"

#include <SDL3/SDL_assert.h>
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
#include "System/gameTime.h"
#include "Utils/helpers.h"
#include "System/luaInterface.h"

#include "DefaultECPS/generalComponents.h"

// move all of these into separate files?

// ***** Render Process

// assume we're using the general components, just need to know what component ids to use then
void gp_GeneralRender( ECPS* ecps, const Entity* entity, ComponentID tfCompID, ComponentID sprCompID, ComponentID clrCompID, ComponentID floatVal0CompID, ComponentID stencilCompID )
{
	GCTransformData* transform = NULL;
	GCSpriteData* sprite = NULL;

	ecps_GetComponentFromEntity( entity, tfCompID, &transform );
	ecps_GetComponentFromEntity( entity, sprCompID, &sprite );

	float t = gt_GetRenderNormalizedTime( );
	ImageRenderInstruction inst = img_CreateDefaultRenderInstruction( );

	Vector2 imgOffset;
	img_GetOffset( sprite->img, &imgOffset );

	inst.camFlags = sprite->camFlags;
	inst.depth = sprite->depth;
	inst.imgID = sprite->img;
	gc_GetLerpedGlobalMatrix( ecps, transform, &imgOffset, t, &inst.mat );

	GCColorData* colorData = NULL;
	if( ecps_GetComponentFromEntity( entity, clrCompID, &colorData ) ) {
		clr_Lerp( &( colorData->currClr ), &( colorData->futureClr ), t, &( inst.color ) );
	}

	GCFloatVal0Data* val0Data = NULL;
	if( ecps_GetComponentFromEntity( entity, floatVal0CompID, &val0Data ) ) {
		inst.val0 = lerp( val0Data->currValue, val0Data->futureValue, t );
	}

	GCStencilData* stencilData = NULL;
	if( ecps_GetComponentFromEntity( entity, stencilCompID, &stencilData ) ) {
		inst.isStencil = stencilData->isStencil;
		inst.stencilID = stencilData->stencilID;
	}

	img_ImmediateRender( &inst );
}


Process gpRenderProc;
static void render( ECPS* ecps, const Entity* entity )
{
	gp_GeneralRender( ecps, entity, gcTransformCompID, gcSpriteCompID, gcClrCompID, gcFloatVal0CompID, gcStencilCompID );
}

// ***** Render 3x3 Process
//  Is there a way to wrap this up into the general render process?
// TODO: Set this to use the size component instead of using the scale as the size
Process gp3x3RenderProc;
static void render3x3( ECPS* ecps, const Entity* entity )
{
	/*
	* 0 1 2
	* 3 4 5
	* 6 7 8
	*/
	Vector2 offsets[9];
	Vector2 scales[9];
	GCTransformData* transform = NULL;
	GC3x3SpriteData* sprite = NULL;

	ecps_GetComponentFromEntity( entity, gcTransformCompID, &transform );
	ecps_GetComponentFromEntity( entity, gc3x3SpriteCompID, &sprite );

	float t = gt_GetRenderNormalizedTime( );

	// create the offsets for each image, we're assuming we won't be using the image offsets for 3x3s
	// assuming that the dimensions of each column and row are the same
	// also assumes the width and height to draw greater than the sum of the widths and heights, if it isn't we just
	//  clamp it so it's a minimum of that much
	Vector2 renderScale;
	vec2_Lerp( &( transform->currState.scale ), &( transform->futureState.scale ), t, &renderScale );

	Vector2 sliceSize;
	img_GetSize( sprite->imgs[0], &sliceSize );
	float imgLeftColumnWidth = sliceSize.x * renderScale.x;
	float imgTopRowHeight = sliceSize.y * renderScale.y;

	img_GetSize( sprite->imgs[4], &sliceSize );
	float imgMidColumnWidth = sliceSize.x * renderScale.x;
	float imgMidRowHeight = sliceSize.y * renderScale.y;

	img_GetSize( sprite->imgs[8], &sliceSize );
	float imgRightColumnWidth = sliceSize.x * renderScale.x;
	float imgBottomRowHeight = sliceSize.y * renderScale.y;

	// calculate the scales
	float midWidth = MAX( 0.0f, ( ( sprite->size.x * renderScale.x ) - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthScale = midWidth / imgMidColumnWidth;

	float midHeight = MAX( 0.0f, ( ( sprite->size.y * renderScale.y ) - imgTopRowHeight - imgBottomRowHeight ) );
	float midHeightScale = midHeight / imgMidRowHeight;

	// set the scale to the base scale from size calculations then apply the transform scale
	scales[0] = scales[2] = scales[6] = scales[8] = VEC2_ONE;
	scales[3].x = scales[5].x = scales[1].y = scales[7].y = 1.0f;
	scales[1].x = scales[4].x = scales[7].x = midWidthScale;
	scales[3].y = scales[4].y = scales[5].y = midHeightScale;
	for( int i = 0; i < 9; ++i ) {
		vec2_HadamardProd( &( scales[i] ), &( transform->currState.scale ), &( scales[i] ) );
	}

	// calculate the offsets
	offsets[0].x = offsets[3].x = offsets[6].x = -( midWidth / 2.0f ) - ( imgLeftColumnWidth / 2.0f );
	offsets[1].x = offsets[4].x = offsets[7].x = 0.0f;
	offsets[2].x = offsets[5].x = offsets[8].x = ( midWidth / 2.0f ) + ( imgRightColumnWidth / 2.0f );

	offsets[0].y = offsets[1].y = offsets[2].y = -( midHeight / 2.0f ) - ( imgTopRowHeight / 2.0f );
	offsets[3].y = offsets[4].y = offsets[5].y = 0.0f;
	offsets[6].y = offsets[7].y = offsets[8].y = ( midHeight / 2.0f ) + ( imgBottomRowHeight / 2.0f );

	Vector2 storedCurrScale = transform->currState.scale;
	Vector2 storedFutureScale = transform->futureState.scale;
	for( int i = 0; i < 9; ++i ) {
		ImageRenderInstruction inst = img_CreateDefaultRenderInstruction( );

		transform->currState.scale = scales[i];
		transform->futureState.scale = scales[i];

		inst.camFlags = sprite->camFlags;
		inst.depth = sprite->depth;
		inst.imgID = sprite->imgs[i];
		gc_GetLerpedGlobalMatrix( ecps, transform, &offsets[i], t, &inst.mat );

		GCColorData* colorData = NULL;
		if( ecps_GetComponentFromEntity( entity, gcClrCompID, &colorData ) ) {
			clr_Lerp( &( colorData->currClr ), &( colorData->futureClr ), t, &( inst.color ) );
		}

		GCFloatVal0Data* val0Data = NULL;
		if( ecps_GetComponentFromEntity( entity, gcFloatVal0CompID, &val0Data ) ) {
			inst.val0 = lerp( val0Data->currValue, val0Data->futureValue, t );
		}

		GCStencilData* stencilData = NULL;
		if( ecps_GetComponentFromEntity( entity, gcStencilCompID, &stencilData ) ) {
			inst.isStencil = stencilData->isStencil;
			inst.stencilID = stencilData->stencilID;
		}

		img_ImmediateRender( &inst );
	}
	transform->currState.scale = storedCurrScale;
	transform->futureState.scale = storedFutureScale;
}

// ***** Render Text Boxes Process
// TODO: Get this working better
Process gpTextRenderProc;
static void renderText( ECPS* ecps, const Entity* entity )
{
	GCTextData* txt = NULL;
	GCTransformData* tf = NULL;
	GCSizeData* size = NULL;

	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf );
	ecps_GetComponentFromEntity( entity, gcTextCompID, &txt );
	ecps_GetComponentFromEntity( entity, gcSizeCompID, &size );

	float t = gt_GetRenderNormalizedTime( );
	Matrix3 baseMatrix;
	gc_GetLerpedGlobalMatrix( ecps, tf, &VEC2_ZERO, t, &baseMatrix );

	Vector2 currSize;
	vec2_Lerp( &(size->currentSize), &(size->futureSize), t, &currSize );

	txt_DisplayTextArea( txt->text, &baseMatrix, currSize, txt->clr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, txt->fontID, 0, NULL, txt->camFlags, txt->depth, txt->pixelSize );
}

// ***** Render Swap Process
void gp_GeneralSwap( ECPS* epcs, const Entity* entity, ComponentID tfCompID, ComponentID clrCompID, ComponentID floatVal0CompID )
{
	GCTransformData* transform = NULL;
	ecps_GetComponentFromEntity( entity, tfCompID, &transform );
	gc_SwapTransformStates( transform );

	GCColorData* color = NULL;
	if( ecps_GetComponentFromEntity( entity, clrCompID, &color ) ) {
		color->currClr = color->futureClr;
	}

	GCFloatVal0Data* floatVal0 = NULL;
	if( ecps_GetComponentFromEntity( entity, floatVal0CompID, &floatVal0 ) ) {
		floatVal0->currValue = floatVal0->futureValue;
	}
}

Process gpRenderSwapProc;
static void renderSwap( ECPS* ecps, const Entity* entity )
{
	gp_GeneralSwap( ecps, entity, gcTransformCompID, gcClrCompID, gcFloatVal0CompID );
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

	GCPointerCollisionData* responseData = NULL;
	ecps_GetComponentFromEntity( entity, gcPointerCollisionCompID, &responseData );
	if( responseData->priority < currChosenPriority ) {
		return;
	}

	GCTransformData* tfData = NULL;
	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tfData );

	Vector3 diff;
	for( size_t i = 0; i < sb_Count( sbResponseMousePos ); ++i ) {
		if( !( responseData->camFlags & sbResponseMousePos[i].camFlags ) ) continue;

		// see if any of the local mouse pointer is in this entity
		//prevState = responseData->state;
		diff.x = tfData->currState.pos.x - sbResponseMousePos[i].adjPos.x;
		diff.y = tfData->currState.pos.y - sbResponseMousePos[i].adjPos.y;
		if( ( fabsf( diff.x ) <= responseData->collisionHalfDim.x ) && ( fabsf( diff.y ) <= responseData->collisionHalfDim.y ) ) {
			currChosenPriority = responseData->priority;
			currChosenPointerResponseID = entity->id;
		}
	}
}

static void callOverReponse( ECPS* ecps, EntityID id )
{
	Entity entity;
	GCPointerResponseData* prd = NULL;
	if( ecps_GetEntityAndComponentByID( ecps, id, gcPointerResponseCompID, &entity, &prd ) ) {
		callGeneralCallback( &prd->overResponse, ecps, &entity );
	}
}

static void callLeaveResponse( ECPS* ecps, EntityID id )
{
	Entity entity;
	GCPointerResponseData* prd = NULL;
	if( ecps_GetEntityAndComponentByID( ecps, id, gcPointerResponseCompID, &entity, &prd ) ) {
		callGeneralCallback( &prd->leaveResponse, ecps, &entity );
	}
}

static void callPressResponse( ECPS* ecps, EntityID id )
{
	Entity entity;
	GCPointerResponseData* prd = NULL;
	if( ecps_GetEntityAndComponentByID( ecps, id, gcPointerResponseCompID, &entity, &prd ) ) {
		callGeneralCallback( &prd->pressResponse, ecps, &entity );
	}
}

static void callReleaseReponse( ECPS* ecps, EntityID id )
{
	Entity entity;
	GCPointerResponseData* prd = NULL;
	if( ecps_GetEntityAndComponentByID( ecps, id, gcPointerResponseCompID, &entity, &prd ) ) {
		callGeneralCallback( &prd->releaseResponse, ecps, &entity );
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

	if( ( pointerResponseState == PRS_IDLE ) && ( currChosenPointerResponseID != INVALID_ENTITY_ID ) && pointerResponseMousePressed ) {
		// touch is currently over chosen entity, switch from idle to over, call over and press response
		pointerResponseState = PRS_CLICKED_OVER;
		focusedPointerResponseID = currChosenPointerResponseID;

		callOverReponse( ecps, currChosenPointerResponseID );
		callPressResponse( ecps, currChosenPointerResponseID );
	}

	if( pointerResponseState == PRS_CLICKED_OVER ) {
		if( currChosenPointerResponseID != focusedPointerResponseID ) {
			// finger was down and is no longer over, leave response
			pointerResponseState = PRS_CLICKED_NOT_OVER;

			callLeaveResponse( ecps, currChosenPointerResponseID );
		} else if( pointerResponseMouseReleased ) {
			// finger released over, call release
			pointerResponseState = PRS_IDLE;

			callReleaseReponse( ecps, currChosenPointerResponseID );
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

			callOverReponse( ecps, currChosenPointerResponseID );
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

	if( pointerResponseState == PRS_OVER ) {
		if( currChosenPointerResponseID != prevChosenPointerResponseID ) {
			pointerResponseState = PRS_IDLE;

			callLeaveResponse( ecps, prevChosenPointerResponseID );
		} else if( pointerResponseMousePressed ) {
			// if the mouse button was pressed, then transition to clicked over state and call the clicked resonse
			focusedPointerResponseID = currChosenPointerResponseID;
			pointerResponseState = PRS_CLICKED_OVER;

			callPressResponse( ecps, currChosenPointerResponseID );
		}
	}

	if( ( pointerResponseState == PRS_IDLE ) && ( currChosenPointerResponseID != INVALID_ENTITY_ID ) ) {
		// mouse is currently over chosen entity, switch from idle to over, call over response
		pointerResponseState = PRS_OVER;

		callOverReponse( ecps, currChosenPointerResponseID );
	}

	if( pointerResponseState == PRS_CLICKED_OVER ) {
		if( currChosenPointerResponseID != focusedPointerResponseID ) {
			// mouse was clicked and is no longer over, leave response
			pointerResponseState = PRS_CLICKED_NOT_OVER;

			callLeaveResponse( ecps, currChosenPointerResponseID );
		} else if( pointerResponseMouseReleased ) {
			// mouse released over, call release
			pointerResponseState = PRS_OVER;

			callReleaseReponse( ecps, currChosenPointerResponseID );
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
			
			callOverReponse( ecps, currChosenPointerResponseID );
		}
	}

	prevChosenPointerResponseID = currChosenPointerResponseID;
	pointerResponseMousePressed = false;
	pointerResponseMouseReleased = false;
}

// needed to handle button presses
void gp_PointerResponseEventHandler( SDL_Event * evt )
{
	if( evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN ) {
		if( evt->button.button == SDL_BUTTON_LEFT ) {
			pointerResponseMousePressed = true;
		}
	} else if( evt->type == SDL_EVENT_MOUSE_BUTTON_UP ) {
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

// ***** Draw PointerResponses for debugging
Process gpDebugDrawPointerReponsesProc;
#include "Graphics/debugRendering.h"
static void drawPointerResponse( ECPS* ecps, const Entity* entity )
{
	GCPointerCollisionData* pointer = NULL;
	GCTransformData* tf = NULL;

	ecps_GetComponentFromEntity( entity, gcPointerCollisionCompID, &pointer );
	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf );

	Vector2 topLeft;
	Vector2 fullSize;
	vec2_Subtract( &tf->currState.pos, &pointer->collisionHalfDim, &topLeft );
	vec2_Scale( &pointer->collisionHalfDim, 2.0f, &fullSize );
	//debugRenderer_AABB( pointer->camFlags, topLeft, fullSize, CLR_BLUE );
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
	GCTransformData* tf = NULL;
	GCVec2TweenData* posTween = NULL;

	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf );
	ecps_GetComponentFromEntity( entity, gcPosTweenCompID, &posTween );

	if( runVec2Tween( posTween, &( tf->futureState.pos ) ) ) {
		// done, remove
		ecps_RemoveComponentFromEntityByID( ecps, entity->id, gcPosTweenCompID );
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
	GCTransformData* tf = NULL;
	GCVec2TweenData* scaleTween = NULL;

	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf );
	ecps_GetComponentFromEntity( entity, gcScaleTweenCompID, &scaleTween );

	if( runVec2Tween( scaleTween, &( tf->futureState.scale ) ) ) {
		// done, remove
		ecps_RemoveComponentFromEntityByID( ecps, entity->id, gcScaleTweenCompID );
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
		ecps_RemoveComponentFromEntityByID( ecps, entity->id, gcAlphaTweenCompID );
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

// ***** Collision processes
// This is an example of a gather-run process. We first gather all the collisions, then run the collision detection at the end phase of the process.
// TODO: Make this more configurable.
Process gpCollisionProc;
typedef struct CollisionEntry {
	Collider coll;
	EntityID id;
} CollisionEntry;

static CollisionEntry* colliders = NULL;
static ECPS* collisionECPS;

static void clearColliders( ECPS* ecps )
{
	sb_Clear( colliders );
	collisionECPS = ecps;
}

static void addCollider( ECPS* ecps, const Entity* entity )
{
	GCColliderData* collData = NULL;
	GCTransformData* tf = NULL;

	ecps_GetComponentFromEntity( entity, gcColliderCompID, &collData );
	ecps_GetComponentFromEntity( entity, gcTransformCompID, &tf );

	CollisionEntry newEntry;
	newEntry.id = entity->id;
	gc_ColliderDataToCollider( collData, tf, &(newEntry.coll) );
	sb_Push( colliders, newEntry );
}

static void exampleCollisionResponse( Entity* eOne, Entity* eTwo )
{
	// eOne will have firstCompID, eTwo will have secondCompID
	SDL_assert( false && "Example only, make you're own." );
}

bool collisionTestAndResponse( Entity* eOne, Entity* eTwo, ComponentID firstCompID, ComponentID secondCompID, void ( *response )( Entity*, Entity* ) )
{
	if( ecps_DoesEntityHaveComponent( eOne, firstCompID ) && ecps_DoesEntityHaveComponent( eTwo, secondCompID ) ) {
		response( eOne, eTwo );
		return true;
	} else if( ecps_DoesEntityHaveComponent( eTwo, firstCompID ) && ecps_DoesEntityHaveComponent( eOne, secondCompID ) ) {
		response( eTwo, eOne );
		return true;
	}
	return false;
}

void collisionResponse( int firstColliderIdx, int secondColliderIdx, Vector2 separation )
{
	Entity eOne;
	Entity eTwo;

	if( !ecps_GetEntityByID( collisionECPS, colliders[firstColliderIdx].id, &eOne ) ) return;
	if( !ecps_GetEntityByID( collisionECPS, colliders[secondColliderIdx].id, &eTwo ) ) return;

	// add responses to collisions here
	//if( collisionTestAndResponse( &eOne, &eTwo, firstCompID, secondCompID, exampleCollisionResponse ) ) return;
}

static void runCollisions( ECPS* ecps )
{
	ColliderCollection coll;
	coll.firstCollider = &colliders[0].coll;
	coll.count = sb_Count( colliders );
	coll.stride = sizeof( colliders[0] );
	collision_DetectAllInternal( coll, collisionResponse );
}

// ***** Short lived process
Process gpShortLivedProc;
static void shortLivedUpdate( ECPS* ecps, const Entity* entity )
{
	GCShortLivedData* shortLived = NULL;

	ecps_GetComponentFromEntity( entity, gcShortLivedCompID, &shortLived );

	shortLived->timePassed += PHYSICS_DT;
	if( shortLived->timePassed >= shortLived->duration ) {
		// done, destroy
		callGeneralCallback( &shortLived->onDestroyedCallback, ecps, (Entity*)entity );
		ecps_DestroyEntity( ecps, entity );
	}
}

void gp_AddShortLived( ECPS* ecps, EntityID entity, float duration )
{
	GCShortLivedData shortLived;
	shortLived.duration = duration;
	shortLived.timePassed = 0.0f;
	shortLived.onDestroyedCallback = createSourceCallback( NULL );
	ecps_AddComponentToEntityByID( ecps, entity, gcShortLivedCompID, &shortLived );
}

// ***** Sprite animation process
Process gpAnimSpriteProc;
static void animSpriteUpdate( ECPS* ecps, const Entity* entity )
{
	GCAnimatedSpriteData* anim = NULL;

	ecps_GetComponentFromEntity( entity, gcAnimSpriteCompID, &anim );

	AnimSpriteHandlerData handlerData;
	handlerData.ecps = ecps;
	handlerData.id = entity->id;
	anim->eventHandler.data = &handlerData;

	float dt = PHYSICS_DT * anim->playSpeedScale;

	sprAnim_ProcessAnim( &(anim->playingAnim), &(anim->eventHandler), dt );

	anim->eventHandler.data = NULL;
}

// ***** Helper functions for dealing with groups
void gp_AddGroupID( ECPS* ecps, EntityID entity, uint32_t groupID )
{
	GCGroupIDData* groupIDData = NULL;
	if( ecps_GetComponentFromEntityByID( ecps, entity, gcGroupIDCompID, &groupIDData ) ) {
		// set the group id if the component already exists
		groupIDData->groupID = groupID;
	} else {
		// add the group id if the entity doesn't have the component
		GCGroupIDData newGroupIDData;
		newGroupIDData.groupID = groupID;
		ecps_AddComponentToEntityByID( ecps, entity, gcGroupIDCompID, &newGroupIDData );
	}
}

void gp_AddGroupIDToEntityAndChildren( ECPS* ecps, EntityID rootEntityID, uint32_t groupID )
{
	ASSERT_AND_IF_NOT( ecps != NULL ) return;

	gp_AddGroupID( ecps, rootEntityID, groupID );

	GCTransformData* tfData = NULL;
	if( !ecps_GetComponentFromEntityByID( ecps, rootEntityID, gcTransformCompID, &tfData ) ) {
		return;
	}

	EntityID child = tfData->firstChildID;
	while( child != INVALID_ENTITY_ID ) {
		GCTransformData* childTfData = NULL;
		ecps_GetComponentFromEntityByID( ecps, child, gcTransformCompID, &childTfData );
		ASSERT_AND_IF_NOT( childTfData != NULL )
		{
			// found a mounted entity that does not have a transform component attached to it
			child = INVALID_ENTITY_ID;
		} else {
			gp_AddGroupIDToEntityAndChildren( ecps, child, groupID );
			child = childTfData->nextSiblingID;
		}
	}
}

static uint32_t deleteGroupID = SIZE_MAX;
static void testAndDeleteEntity( ECPS* ecps, const Entity* entity )
{
	GCGroupIDData* groupID = NULL;
	ecps_GetComponentFromEntity( entity, gcGroupIDCompID, &groupID );
	if(groupID->groupID == deleteGroupID) {
		ecps_DestroyEntity( ecps, entity );
	}
}

void gp_DeleteAllOfGroup( ECPS* ecps, uint32_t groupID )
{
	// just use a custom process
	deleteGroupID = groupID;
	ecps_RunCustomProcess( ecps, NULL, testAndDeleteEntity, NULL, 1, gcGroupIDCompID );
}

// ***** Register the processes
void gp_RegisterProcesses( ECPS* ecps )
{
	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcSpriteCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "DRAW", NULL, render, NULL, &gpRenderProc, 2, gcTransformCompID, gcSpriteCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gc3x3SpriteCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "3x3", NULL, render3x3, NULL, &gp3x3RenderProc, 2, gcTransformCompID, gc3x3SpriteCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcTextCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "TEXT", NULL, renderText, NULL, &gpTextRenderProc, 3, gcTransformCompID, gcTextCompID, gcSizeCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "DRAWSWAP", NULL, renderSwap, NULL, &gpRenderSwapProc, 1, gcTransformCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPointerCollisionCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPointerResponseCompID != INVALID_COMPONENT_ID );
#if defined( __ANDROID__ )
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_TouchScreen, &gpPointerResponseProc, 3, gcTransformCompID, gcPointerCollisionCompID, gcPointerResponseCompID );
#else
	ecps_CreateProcess( ecps, "CLICK", pointerResponseGetMouse, pointerResponseDetectState, pointerResponseFinalize_Mouse, &gpPointerResponseProc, 3, gcTransformCompID, gcPointerCollisionCompID, gcPointerResponseCompID );
#endif

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPointerCollisionCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "CLK_DBG", NULL, drawPointerResponse, NULL, &gpDebugDrawPointerReponsesProc, 2, gcTransformCompID, gcPointerCollisionCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcPosTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "POS_TWEEN", NULL, posTweenUpdate, NULL, &gpPosTweenProc, 2, gcTransformCompID, gcPosTweenCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcScaleTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "SCALE_TWEEN", NULL, scaleTweenUpdate, NULL, &gpScaleTweenProc, 2, gcTransformCompID, gcScaleTweenCompID );

	SDL_assert( gcClrCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcAlphaTweenCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "ALPHA_TWEEN", NULL, alphaTweenUpdate, NULL, &gpAlphaTweenProc, 2, gcClrCompID, gcAlphaTweenCompID );

	SDL_assert( gcTransformCompID != INVALID_COMPONENT_ID );
	SDL_assert( gcColliderCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "COLLISION", clearColliders, addCollider, runCollisions, &gpCollisionProc, 2, gcTransformCompID, gcColliderCompID );

	SDL_assert( gcShortLivedCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "SHORT_LIVED", NULL, shortLivedUpdate, NULL, &gpShortLivedProc, 1, gcShortLivedCompID );

	SDL_assert( gcAnimSpriteCompID != INVALID_COMPONENT_ID );
	ecps_CreateProcess( ecps, "ANIM_SPRITE", NULL, animSpriteUpdate, NULL, &gpAnimSpriteProc, 1, gcAnimSpriteCompID );
}
