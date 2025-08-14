#include "testCollisionsState.h"

#include <float.h>

#include "Graphics/graphics.h"
#include "Graphics/debugRendering.h"
#include "Graphics/camera.h"
#include "System/ECPS/entityComponentProcessSystem.h"
#include "DefaultECPS/generalComponents.h"
#include "DefaultECPS/generalProcesses.h"
#include "DefaultECPS/defaultECPS.h"
#include "System/platformLog.h"
#include "System/gameTime.h"
#include "Utils/stretchyBuffer.h"
#include "collisionDetection.h"
#include "IMGUI/nuklearWrapper.h"
#include "Input/input.h"

static ColliderCollection colliders = { NULL, sizeof( Collider ), 0 };
static Collider clickCollider;
static struct nk_rect uiBounds;

static size_t selectedCollider = SIZE_MAX;

static size_t* sbCollisions = NULL;

static void removeCollider( size_t idx )
{
	sb_Remove( colliders.firstCollider, idx );
	colliders.count = sb_Count( colliders.firstCollider );
}

static Collider* addCollider( void )
{
	Collider newCollider;
	newCollider.type = CT_DEACTIVATED;
	sb_Push( colliders.firstCollider, newCollider );
	colliders.count = sb_Count( colliders.firstCollider );
	selectedCollider = colliders.count - 1;
	return &colliders.firstCollider[colliders.count - 1];
}

static void addAABB( void )
{
	Collider* coll = addCollider( );
	coll->type = CT_AABB;
	coll->aabb.center = VEC2_ZERO;
	coll->aabb.halfDim = vec2( 10.0f, 10.0f );
}

static void addCircle( void )
{
	Collider* coll = addCollider( );
	coll->type = CT_CIRCLE;
	coll->circle.center = VEC2_ZERO;
	coll->circle.radius = 10.0f;
}

static void addHalfSpace( void )
{
	Collider* coll = addCollider( );
	collision_CalculateHalfSpace( &VEC2_ZERO, &VEC2_UP, coll );
}

static void addLineSegment( void )
{
	Collider* coll = addCollider( );
	coll->type = CT_LINE_SEGMENT;
	coll->lineSegment.posOne = vec2( -5.0f, 0.0f );
	coll->lineSegment.posTwo = vec2( 5.0f, 0.0f );
}

static size_t* sbSelectedCollidersList = NULL;
static bool isUIActiveOrHovered = false;
static void clickCollisionResponse( size_t ignore, size_t colliderIdx, Vector2 separation )
{
	sb_Push( sbSelectedCollidersList, colliderIdx );
}

static void chooseCollider( void )
{
	if( isUIActiveOrHovered ) return;

	size_t lastSelectedCollider = selectedCollider;
	selectedCollider = SIZE_MAX;
	sb_Clear( sbSelectedCollidersList );

	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	cam_ScreenPosToWorldPos( 0, &mousePos, &clickCollider.circle.center );

	collision_Detect( &clickCollider, colliders, clickCollisionResponse, SIZE_MAX );

	if( sb_Count( sbSelectedCollidersList ) <= 0 ) return;

	// if the last chosen collider is in the list then choose the one after it, unless we're at the end then choose the first
	size_t lastIdx = SIZE_MAX;
	for( size_t i = 0; i < sb_Count( sbSelectedCollidersList ); ++i ) {
		if( sbSelectedCollidersList[i] == lastSelectedCollider ) {
			lastIdx = i;
		}
	}

	if( lastIdx == SIZE_MAX ) {
		// no current set selected, use the first in the list
		selectedCollider = sbSelectedCollidersList[0];
	} else if( ( lastIdx + 1 ) >= sb_Count( sbSelectedCollidersList ) ) {
		// current selection is last in list, choose first
		selectedCollider = sbSelectedCollidersList[0];
	} else {
		selectedCollider = sbSelectedCollidersList[lastIdx + 1];
	}
}

static void testCollisionsState_Enter( void )
{
	clickCollider.type = CT_CIRCLE;
	clickCollider.circle.radius = 3.0f;

	int width, height;
	gfx_GetRenderSize( &width, &height );
	cam_SetFlags( 0, 1 );
	cam_SetCenteredProjectionMatrix( 0, width, height );

	gfx_SetClearColor( CLR_BLACK );

	input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, chooseCollider );
}

static void testCollisionsState_Exit( void )
{
	sb_Release( sbSelectedCollidersList );
	sb_Release( sbCollisions );
	sb_Release( colliders.firstCollider );
	colliders.count = 0;

	input_ClearAllMouseButtonBinds( );
}

static void testCollisionsState_ProcessEvents( SDL_Event* e )
{
}

static void showAABBUI( struct nk_context* ctx, Collider* coll )
{
	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		// spacer
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Center X", -FLT_MAX, &(coll->aabb.center.x), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Center Y", -FLT_MAX, &( coll->aabb.center.y ), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		coll->aabb.halfDim.w = nk_propertyf( ctx, "Width", -FLT_MAX, coll->aabb.halfDim.w * 2.0f, FLT_MAX, 1.0f, 2.0f ) / 2.0f;
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		coll->aabb.halfDim.h = nk_propertyf( ctx, "Height", -FLT_MAX, coll->aabb.halfDim.h * 2.0f, FLT_MAX, 1.0f, 2.0f ) / 2.0f;
	} nk_layout_row_end( ctx );
}

static void showCircleUI( struct nk_context* ctx, Collider* coll )
{
	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		// spacer
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Center X", -FLT_MAX, &(coll->circle.center.x), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Center Y", -FLT_MAX, &(coll->circle.center.y), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Radius", 1.0f, &(coll->circle.radius), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );
}

static void showHalfSpaceUI( struct nk_context* ctx, Collider* coll )
{
	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		// spacer
	} nk_layout_row_end( ctx );

	Vector2 pos = coll->halfSpace.pos;
	float angleRad = vec2_RotationRadians( &coll->halfSpace.normal );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos X", -FLT_MAX, &pos.x, FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos Y", -FLT_MAX, &pos.y, FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		float angleDeg = RAD_TO_DEG( angleRad );
		nk_property_float( ctx, "Normal Angle", -FLT_MAX, &angleDeg, FLT_MAX, 1.0f, 2.0f );
		angleRad = DEG_TO_RAD( angleDeg );
	} nk_layout_row_end( ctx );

	Vector2 normal;
	vec2_NormalFromRot( angleRad, &normal );

	collision_CalculateHalfSpace( &pos, &normal, coll );
}

static void showLineSegmentUI( struct nk_context* ctx, Collider* coll )
{
	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		// spacer
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos One X", -FLT_MAX, &(coll->lineSegment.posOne.x), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos One Y", -FLT_MAX, &(coll->lineSegment.posOne.y), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos Two X", -FLT_MAX, &( coll->lineSegment.posTwo.x ), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );

	nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
		nk_layout_row_push( ctx, 1.0f );
		nk_property_float( ctx, "Pos Two Y", -FLT_MAX, &( coll->lineSegment.posTwo.y ), FLT_MAX, 1.0f, 2.0f );
	} nk_layout_row_end( ctx );
}

static void testCollisionsState_Process( void )
{
	struct nk_context* ctx = &( inGameIMGUI.ctx );
	int renderWidth, renderHeight;
	gfx_GetRenderSize( &renderWidth, &renderHeight );
	struct nk_rect windowBounds = { 0.0f, 0.0f, 200.0f, (float)renderHeight };
	if( nk_begin( ctx, "Collisions", windowBounds, NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE ) ) {

		uiBounds = nk_window_get_bounds( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_button_label( ctx, "Create AABB Collision" ) ) {
				addAABB( );
			}
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_button_label( ctx, "Create Circle Collision" ) ) {
				addCircle( );
			}
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_button_label( ctx, "Create Halfspace Collision" ) ) {
				addHalfSpace( );
			}
		} nk_layout_row_end( ctx );

		nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
			nk_layout_row_push( ctx, 1.0f );
			if( nk_button_label( ctx, "Create Line Segment Collision" ) ) {
				addLineSegment( );
			}
		} nk_layout_row_end( ctx );

		if( selectedCollider != SIZE_MAX ) {
			switch( colliders.firstCollider[selectedCollider].type ) {
			case CT_AABB:
				showAABBUI( ctx, &colliders.firstCollider[selectedCollider] );
				break;
			case CT_CIRCLE:
				showCircleUI( ctx, &colliders.firstCollider[selectedCollider] );
				break;
			case CT_HALF_SPACE:
				showHalfSpaceUI( ctx, &colliders.firstCollider[selectedCollider] );
				break;
			case CT_LINE_SEGMENT:
				showLineSegmentUI( ctx, &colliders.firstCollider[selectedCollider] );
				break;
			}

			nk_layout_row_begin( ctx, NK_DYNAMIC, 30, 1 ); {
				nk_layout_row_push( ctx, 1.0f );
				if( nk_button_label( ctx, "Delete Collider" ) ) {
					removeCollider( selectedCollider );
					selectedCollider = SIZE_MAX;
				}
			} nk_layout_row_end( ctx );
		}

		isUIActiveOrHovered = nk_item_is_any_active( ctx );
	} nk_end( ctx );
}

static void collisionResponse( size_t firstColliderIdx, size_t secondColliderIdx, Vector2 separation )
{
	sb_Push( sbCollisions, firstColliderIdx );
	sb_Push( sbCollisions, secondColliderIdx );
}

static bool doesCollide( size_t idx )
{
	for( size_t i = 0; i < sb_Count( sbCollisions ); ++i ) {
		if( sbCollisions[i] == idx ) return true;
	}
	return false;
}

static void testCollisionsState_Draw( void )
{
	sb_Clear( sbCollisions );
	collision_DetectAllInternal( colliders, collisionResponse );

	for( size_t i = 0; i < colliders.count; ++i ) {
		Color color;
		Color nonChosenColor = CLR_BLUE;
		Color chosenColor = CLR_CYAN;

		if( doesCollide( i ) ) {
			nonChosenColor = CLR_RED;
			chosenColor = CLR_YELLOW;
		}

		color = nonChosenColor;
		if( i == selectedCollider ) {
			color = chosenColor;
		}
		collision_DebugDrawing( &colliders.firstCollider[i], 1, color );
	}

	//collision_DebugDrawing( &clickCollider, 1, CLR_WHITE );
}

static void testCollisionsState_PhysicsTick( float dt )
{
}

GameState testCollisionsState = { testCollisionsState_Enter, testCollisionsState_Exit, testCollisionsState_ProcessEvents,
	testCollisionsState_Process, testCollisionsState_Draw, testCollisionsState_PhysicsTick };
