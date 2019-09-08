#ifndef HEX_GRID
#define HEX_GRID

#include <stdint.h>
#include "../Math/vector2.h"

/*
https://www.redblobgames.com/grids/hexagons/

For right now lets assume the storage is external, we just need a way to turn a coordinate into
 an index.

Essentially we're just getting a bunch of functions that can be used to do some things about a hex grid
 and not with a hex grid.
*/

typedef enum {
	HD_F_NORTH,
	HD_F_NORTH_EAST,
	HD_F_SOUTH_EAST,
	HD_F_SOUTH,
	HD_F_SOUTH_WEST,
	HD_F_NORTH_WEST
} HexDirection_Flat;

typedef enum {
	HD_P_NORTH_WEST,
	HD_P_NORTH_EAST,
	HD_P_EAST,
	HD_P_SOUTH_EAST,
	HD_P_SOUTH_WEST,
	HD_P_WEST,
} HexDirection_Pointy;

// we'll use axial coordinates
typedef struct {
	int32_t q;
	int32_t r;
} HexGridCoord;

// we're assuming the grid coordinate [0,0] is the position [0,0]
Vector2 hex_Flat_GridToPosition( float size, HexGridCoord coord );
HexGridCoord hex_Flat_PositionToGrid( float size, Vector2 pos );
Vector2 hex_Pointy_GridToPosition( float size, HexGridCoord coord );
HexGridCoord hex_Pointy_PositionToGrid( float size, Vector2 pos );

// For flat topped the neighbor at 0 will be at the top, for pointy topped the neighbor at 0 will be to the north-west
//  both will continue clockwise around the hex.
// You can use the HexDirection_Flat and HexDirection_Pointy enumerations to more easily access the one you want.
HexGridCoord hex_GetNeighbor( HexGridCoord baseCoord, int neighbor );

// Returns the distance in steps from the two grid coordinates.
int32_t hex_Distance( HexGridCoord from, HexGridCoord to );

// Get all hex coords within range steps of base and put them into the stretchy buffer sbOutList
void hex_AllInRange( HexGridCoord base, int32_t range, HexGridCoord** sbOutList );

// Get all hex coords along a line between two hex coords and put them into the stretchy buffer sbOutList
void hex_AllInLine( HexGridCoord start, HexGridCoord end, HexGridCoord** sbOutList );

// Get all hex coords that are a certain range from the center, where range > 0, and puts them into the stretchy buffer sbOutList
void hex_Ring( HexGridCoord center, int32_t range, HexGridCoord** sbOutList );

//***************************************************************************
// Various functions to convert hex coordinates to grid indices

// hex <-> rect, pass in the width and height of the rectangular array
//  assuming information is densely packed and there is no empty space
bool hex_CoordInRect( HexGridCoord coord, int32_t width, int32_t height );
uint32_t hex_CoordToRectIndex( HexGridCoord coord, int32_t width, int32_t height );
HexGridCoord hex_RectIndexToCoord( uint32_t index, int32_t width, int32_t height );

#endif