#include "testAStarScreen.h"

#include <math.h>

#include "Utils/aStar.h"

#include "Graphics/graphics.h"
#include "Graphics/images.h"
#include "Graphics/camera.h"
#include "Graphics/debugRendering.h"
#include "Graphics/imageSheets.h"
#include "UI/text.h"
#include "Utils/helpers.h"
#include "Input/input.h"
#include "System/platformLog.h"

#include "Utils/stretchyBuffer.h"

#include "System/random.h"

static int tileImg;
static int tileMarkerImg;
static int hiliteImg;
static int pathMarkerImg;

typedef enum {
	TT_OPEN,
	TT_CLOSED,
	TT_STICKY,
	TILE_TYPES_COUNT
} TileTypes;

#define MAP_WIDTH 20
#define MAP_HEIGHT 18

#define STICKY_MOVE_ADJ 2.0f

static TileTypes map[MAP_WIDTH*MAP_HEIGHT];

static Vector2 tileSize;

static int mouseOver;

static int startIdx;
static int exitIdx;

static int* sbPath;

static int mapCoordToIdx( int xc, int yc )
{
	return xc + ( yc * MAP_WIDTH );
}

static int mapPosToIdx( Vector2 pos )
{
	pos.x /= tileSize.w;
	pos.y /= tileSize.h;

	if( ( pos.x < 0.0f ) || ( pos.x >= MAP_WIDTH ) ||
		( pos.y < 0.0f ) || ( pos.y >= MAP_HEIGHT ) ) {
		return -1;
	}

	return mapCoordToIdx( (int)pos.x, (int)pos.y );
}

static Vector2 mapIdxToPos( int idx )
{
	int x = idx % MAP_WIDTH;
	int y = idx / MAP_WIDTH;

	Vector2 pos;

	pos.x = ( tileSize.x / 2.0f ) + ( x * tileSize.x );
	pos.y = ( tileSize.y / 2.0f ) + ( y * tileSize.y );

	return pos;
}

static void mapIdxToCoord( int idx, int* outXC, int* outYC )
{
	(*outXC) = idx % MAP_WIDTH;
	(*outYC) = idx / MAP_WIDTH;
}

static void drawMap( void )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	int mouseIdx = mapPosToIdx( mousePos );
	if( mouseIdx >= 0 ) {
		Vector2 pos = mapIdxToPos( mouseIdx );
		img_Render_PosClr( hiliteImg, 1, 10, &pos, &CLR_WHITE );
	}

	for( int i = 0; i < ARRAY_SIZE( map ); ++i ) {
		Vector2 pos = mapIdxToPos( i );

		Color clr = CLR_MAGENTA;
		switch( map[i] ) {
		case TT_OPEN:
			clr = CLR_WHITE;
			break;
		case TT_CLOSED:
			clr = CLR_DARK_GREY;
			break;
		case TT_STICKY:
			clr = CLR_CYAN;
			break;
		}

		img_Render_PosClr( tileImg, 1, 0, &pos, &clr );
	}

	if( startIdx >= 0 ) {
		Vector2 pos = mapIdxToPos( startIdx );
		img_Render_PosClr( tileMarkerImg, 1, 5, &pos, &CLR_GREEN );
	}

	if( exitIdx >= 0 ) {
		Vector2 pos = mapIdxToPos( exitIdx );
		img_Render_PosClr( tileMarkerImg, 1, 5, &pos, &CLR_RED );
	}

	for( size_t i = 0; i < sb_Count( sbPath ); ++i ) {
		Vector2 pos = mapIdxToPos( sbPath[i] );
		img_Render_PosClr( tileMarkerImg, 1, 5, &pos, &CLR_BLUE );
	}
}

static float moveCost( void* graph, int fromIdx, int toIdx )
{
	float cost = 1.0f;
	if( map[toIdx] == TT_STICKY ) {
		cost *= STICKY_MOVE_ADJ;
	} else if( map[toIdx] == TT_CLOSED ) {
		cost = INFINITY;
	}

	return cost;
}

static float heuristic( void* graph, int fromIdx, int toIdx )
{
	// get the coords, then use the manhattan distance
	if( ( map[toIdx] == TT_CLOSED ) || ( map[fromIdx] == TT_CLOSED ) ) {
		return INFINITY;
	}

	int fromXC, fromYC;
	int toXC, toYC;

	mapIdxToCoord( fromIdx, &fromXC, &fromYC );
	mapIdxToCoord( toIdx, &toXC, &toYC );

	return (float)( abs( fromXC - toXC ) + abs( fromYC - toYC ) );
}

static int nextNeighbor( void* graph, int node, int currNeighbor )
{
	int xc, yc;
	mapIdxToCoord( node, &xc, &yc );

	// generate the list of neighbors
	int neighbors[4] = { -1, -1, -1, -1 };
	int lastNeighbor = -1;
#define ADD_NEIGHBOR( n ) neighbors[++lastNeighbor] = (n)

	// up
	if( yc > 0 ) ADD_NEIGHBOR( mapCoordToIdx( xc, yc - 1 ) );
	// left
	if( xc > 0 ) ADD_NEIGHBOR( mapCoordToIdx( xc - 1, yc ) );
	// right
	if( xc < ( MAP_WIDTH - 1 ) ) ADD_NEIGHBOR( mapCoordToIdx( xc + 1, yc ) );
	// down
	if( yc < ( MAP_HEIGHT - 1 ) ) ADD_NEIGHBOR( mapCoordToIdx( xc, yc + 1 ) );

	if( currNeighbor == -1 ) {
		return neighbors[0];
	}

	for( int i = 0; i < lastNeighbor; ++i ) {
		if( neighbors[i] == currNeighbor ) {
			if( neighbors[i+1] >= ( MAP_WIDTH * MAP_HEIGHT ) ) {
				int x = 0;
			}
			return neighbors[i+1];
		}
	}

	return -1;
#undef ADD_NEIGHBOR
}

static void searchForPath( void )
{
	sb_Clear( sbPath );

	AStarSearchState state;

	aStar_CreateSearchState( NULL, MAP_WIDTH * MAP_HEIGHT, startIdx, exitIdx,
		moveCost, heuristic, nextNeighbor, &state );
	aStar_ProcessPath( &state, -1, &sbPath );
	aStar_CleanUpSearchState( &state );
}

static void setCurrentStart( void )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	startIdx = mapPosToIdx( mousePos );

	searchForPath( );
}

static void setCurrentEnd( void )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	exitIdx = mapPosToIdx( mousePos );

	searchForPath( );
}

static void setCurrent( TileTypes type )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	int idx = mapPosToIdx( mousePos );
	if( idx < 0 ) return;

	map[idx] = type;

	searchForPath( );
}

static void setCurrentOpen( void )
{
	setCurrent( TT_OPEN );
}

static void setCurrentSticky( void )
{
	setCurrent( TT_STICKY );
}

static void setCurrentClosed( void )
{
	setCurrent( TT_CLOSED );
}

static void resetMap( void )
{
	for( int i = 0; i < ( MAP_WIDTH * MAP_HEIGHT ); ++i ) {
		map[i] = TT_OPEN;
	}

	startIdx = 0;
	exitIdx = (MAP_WIDTH * MAP_HEIGHT) - 1;
}

static void resetAndSearch( void )
{
	resetMap( );
	searchForPath( );
}

static void generateMaze( void )
{
	resetMap( );

	// for right now just generate noise

	for( int i = 0; i < ( MAP_WIDTH * MAP_HEIGHT ); ++i ) {
		if( ( rand_GetS32( NULL ) % 3 ) == 0 ) {
			map[i] = TT_CLOSED;
		} else if( ( rand_GetS32( NULL ) % 4 ) == 0 ) {
			map[i] = TT_STICKY;
		}
	}

	map[startIdx] = TT_OPEN;
	map[exitIdx] = TT_OPEN;

	searchForPath( );
}

static void testAStarScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( CLR_BLACK );

	tileMarkerImg = img_Load( "Images/marker.png", ST_DEFAULT );
	tileImg = img_Load( "Images/tile.png", ST_DEFAULT );
	hiliteImg = img_Load( "Images/hilite.png", ST_DEFAULT );
	pathMarkerImg = img_Load( "Images/path_marker.png", ST_DEFAULT );
	img_GetSize( tileImg, &tileSize );

	for( int i = 0; i < ( MAP_WIDTH * MAP_HEIGHT ); ++i ) {
		map[i] = TT_OPEN;
	}

	startIdx = 0;
	exitIdx = (MAP_WIDTH * MAP_HEIGHT) - 1;

	mouseOver = -1;

	llog( LOG_DEBUG, "Controls:" );
	llog( LOG_DEBUG, " z - Set start" );
	llog( LOG_DEBUG, " x - Set end" );
	llog( LOG_DEBUG, " q - Set open" );
	llog( LOG_DEBUG, " w - Set sticky" );
	llog( LOG_DEBUG, " e - Set closed" );
	llog( LOG_DEBUG, " a - Generate maze" );
	llog( LOG_DEBUG, " s - Reset and search" );

	input_BindOnKeyPress( SDLK_Z, setCurrentStart );
	input_BindOnKeyPress( SDLK_X, setCurrentEnd );
	input_BindOnKeyPress( SDLK_Q, setCurrentOpen );
	input_BindOnKeyPress( SDLK_W, setCurrentSticky );
	input_BindOnKeyPress( SDLK_E, setCurrentClosed );
	input_BindOnKeyPress( SDLK_A, generateMaze );
	input_BindOnKeyPress( SDLK_S, resetAndSearch );

	sbPath = NULL;

	searchForPath( );
}

static void testAStarScreen_Exit( void )
{
	img_Clean( tileImg );
	img_Clean( tileMarkerImg );
	img_Clean( hiliteImg );
	img_Clean( pathMarkerImg );

	input_ClearAllKeyBinds( );

	sb_Release( sbPath );
}

static void testAStarScreen_ProcessEvents( SDL_Event* e )
{
}

static void testAStarScreen_Process( void )
{
}

static void testAStarScreen_Draw( void )
{
}

static void testAStarScreen_PhysicsTick( float dt )
{
}

static void testAStarScreen_Render( float t )
{
	drawMap( );
}

GameState testAStarScreenState = { testAStarScreen_Enter, testAStarScreen_Exit, testAStarScreen_ProcessEvents,
	testAStarScreen_Process, testAStarScreen_Draw, testAStarScreen_PhysicsTick, testAStarScreen_Render };
