#include "hexTestScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../Input/input.h"
#include "../System/platformLog.h"

#include "../Utils/hexGrid.h"
#include "../Utils/stretchyBuffer.h"

// just draw a 7x7 rectangular grid of hexes

static int greenHexImg;
static int blueHexImg;
static int greyHexImg;
static int brownHexImg;
static int hexHiliteImg;

static Vector2 basePos;

#define GRID_WIDTH 7
#define GRID_HEIGHT 6

#define POINTY_SIZE ( 116.0f / 2.0f )

static int hexMap[GRID_WIDTH * GRID_HEIGHT];

static int radiusSize = 0;

static HexGridCoord lineStart;
static HexGridCoord lineEnd;

static bool getMouseHexCoord( HexGridCoord* c )
{
	Vector2 mousePos;
	if( input_GetMousePosition( &mousePos ) ) {
		vec2_Subtract( &mousePos, &basePos, &mousePos );
		(*c) = hex_Pointy_PositionToGrid( POINTY_SIZE, mousePos );
		if( hex_CoordInRect( (*c), GRID_WIDTH, GRID_HEIGHT ) ) {
			return true;
		}
	}

	return false;
}

static void onMouseClick( void )
{
	HexGridCoord c;
	if( getMouseHexCoord( &c ) ) {
		uint32_t idx = hex_CoordToRectIndex( c, GRID_WIDTH, GRID_HEIGHT );

		if( hexMap[idx] == greenHexImg ) {
			hexMap[idx] = blueHexImg;
		} else if( hexMap[idx] == blueHexImg ) {
			hexMap[idx] = greyHexImg;
		} else if( hexMap[idx] == greyHexImg ) {
			hexMap[idx] = brownHexImg;
		} else {
			hexMap[idx] = greenHexImg;
		}
	}
}

static void increaseRadius( void )
{
	++radiusSize;
}

static void decreaseRadius( void )
{
	radiusSize = MAX( 0, radiusSize - 1 );
}

static void setLineStart( void )
{
	HexGridCoord c;
	if( getMouseHexCoord( &c ) ) {
		lineStart = c;
	}
}

static void setLineEnd( void )
{
	HexGridCoord c;
	if( getMouseHexCoord( &c ) ) {
		lineEnd = c;
	}
}

static int hexTestScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	gfx_SetClearColor( CLR_BLACK );

	greenHexImg = img_Load( "Images/hex_green.png", ST_DEFAULT );
	blueHexImg = img_Load( "Images/hex_blue.png", ST_DEFAULT );
	greyHexImg = img_Load( "Images/hex_grey.png", ST_DEFAULT );
	brownHexImg = img_Load( "Images/hex_brown.png", ST_DEFAULT );
	hexHiliteImg = img_Load( "Images/hex_hilite.png", ST_DEFAULT );

	basePos = vec2( 75.0f, 75.0f );

	for( int i = 0; i < ( GRID_WIDTH * GRID_HEIGHT ); ++i ) {
		switch( i % 4 ) {
		case 0:
			hexMap[i] = greenHexImg;
			break;
		case 1:
			hexMap[i] = blueHexImg;
			break;
		case 2:
			hexMap[i] = brownHexImg;
			break;
		case 3:
			hexMap[i] = greyHexImg;
			break;
		}
	}

	lineStart.q = -1;
	lineStart.r = -1;

	lineEnd.q = -1;
	lineEnd.r = -1;

	input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, onMouseClick );
	input_BindOnKeyPress( SDLK_EQUALS, increaseRadius );
	input_BindOnKeyPress( SDLK_MINUS, decreaseRadius );
	input_BindOnKeyPress( SDLK_q, setLineStart );
	input_BindOnKeyPress( SDLK_w, setLineEnd );

	return 1;
}

static int hexTestScreen_Exit( void )
{
	return 1;
}

static void hexTestScreen_ProcessEvents( SDL_Event* e )
{}

static void hexTestScreen_Process( void )
{}

static void hexTestScreen_Draw( void )
{
	for( int i = 0; i < ( GRID_WIDTH * GRID_HEIGHT ); ++i ) {
		HexGridCoord c = hex_RectIndexToCoord( i, GRID_WIDTH, GRID_HEIGHT );
		Vector2 pos = hex_Pointy_GridToPosition( ( 116.0f / 2.0f ), c );
		vec2_Add( &pos, &basePos, &pos );

		// fixes issues with white noise on the edges
		pos.x = (float)( (int)pos.x );
		pos.y = (float)( (int)pos.y );

		img_CreateDraw( hexMap[i], 1, pos, pos, 0 );
	}

	Vector2 mousePos;
	if( input_GetMousePosition( &mousePos ) ) {
		vec2_Subtract( &mousePos, &basePos, &mousePos );
		HexGridCoord c = hex_Pointy_PositionToGrid( POINTY_SIZE, mousePos );
		if( hex_CoordInRect( c, GRID_WIDTH, GRID_HEIGHT ) ) {

			HexGridCoord* sbList = NULL;
			if( radiusSize > 0 ) hex_Ring( c, radiusSize, &sbList );
			//hex_AllInRange( c, radiusSize, &sbList );

			for( size_t i = 0; i < sb_Count( sbList ); ++i ) {
				if( hex_CoordInRect( sbList[i], GRID_WIDTH, GRID_HEIGHT ) ) {
					Vector2 pos = hex_Pointy_GridToPosition( POINTY_SIZE, sbList[i] );
					vec2_Add( &pos, &basePos, &pos );
					img_CreateDraw( hexHiliteImg, 1, pos, pos, 1 );
				}
			}

			sb_Release( sbList );
		}
	}

	HexGridCoord* sbLine = NULL;
	hex_AllInLine( lineStart, lineEnd, &sbLine );
	for( size_t i = 0; i < sb_Count( sbLine ); ++i ) {
		if( hex_CoordInRect( sbLine[i], GRID_WIDTH, GRID_HEIGHT ) ) {
			Vector2 pos = hex_Pointy_GridToPosition( POINTY_SIZE, sbLine[i] );
			vec2_Add( &pos, &basePos, &pos );
			img_CreateDraw( hexHiliteImg, 1, pos, pos, 1 );
		}
	}
	sb_Release( sbLine );
}

static void hexTestScreen_PhysicsTick( float dt )
{}

GameState hexTestScreenState = { hexTestScreen_Enter, hexTestScreen_Exit, hexTestScreen_ProcessEvents,
	hexTestScreen_Process, hexTestScreen_Draw, hexTestScreen_PhysicsTick };