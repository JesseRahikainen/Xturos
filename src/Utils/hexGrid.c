#include "hexGrid.h"

#include <math.h>
#include <assert.h>

#include "stretchyBuffer.h"
#include "../Math/mathUtil.h"
#include "../System/platformLog.h"

#define SQRT_THREE 1.73205080757f

static HexGridCoord* add( HexGridCoord* lhs, HexGridCoord* rhs, HexGridCoord* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	out->q = lhs->q + rhs->q;
	out->r = lhs->r + rhs->r;

	return out;
}

static HexGridCoord* subtract( HexGridCoord* lhs, HexGridCoord* rhs, HexGridCoord* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	out->q = lhs->q - rhs->q;
	out->r = lhs->r - rhs->r;

	return out;
}

static HexGridCoord cubeToAxial( int32_t x, int32_t y, int32_t z )
{
	HexGridCoord coord;
	coord.q = x;
	coord.r = z;

	return coord;
}

static void axialToCube( HexGridCoord coord, int32_t* outX, int32_t* outY, int32_t* outZ )
{
	( *outX ) = coord.q;
	( *outZ ) = coord.r;
	( *outY ) = -coord.q - coord.r;
}

static HexGridCoord roundHexCoord( float q, float r )
{
	HexGridCoord coord;

	// the recommended way is to convert to cube coordinates, round, and then convert back
	//  axial to cube
	float x = q;
	float z = r;
	float y = -x - z;

	// round
	//  x + y + z = 0 for cubic coordinates
	//  rounding breaks this, so we then take the largest difference and recalculate the
	//  corresponding part of the coordinate based on the other two
	float rx = roundf( x );
	float ry = roundf( y );
	float rz = roundf( z );

	float xDiff = fabsf( rx - x );
	float yDiff = fabsf( ry - y );
	float zDiff = fabsf( rz - z );

	if( ( xDiff > yDiff ) && ( xDiff > zDiff ) ) {
		rx = -ry - rz;
	} else if( yDiff > zDiff ) {
		ry = -rx - rz;
	} else {
		rz = -rx - ry;
	}

	// cube to axial
	coord.q = (int32_t)rx;
	coord.r = (int32_t)rz;

	return coord;
}

// we're assuming the grid coordinate [0,0] is the position [0,0]
Vector2 hex_Flat_GridToPosition( float size, HexGridCoord coord )
{
	Vector2 pos;

	pos.x = size * ( ( 3.0f / 2.0f ) * coord.q );
	pos.y = size * ( ( ( SQRT_THREE / 2.0f ) * coord.q ) + ( SQRT_THREE * coord.r ) );

	return pos;
}

HexGridCoord hex_Flat_PositionToGrid( float size, Vector2 pos )
{
	float q = ( ( 2.0f / 3.0f ) * pos.x ) / size;
	float r = ( ( ( -1.0f / 3.0f ) * pos.x ) + ( ( SQRT_THREE / 3.0f ) * pos.y ) ) / size;

	return roundHexCoord( q, r );
}

Vector2 hex_Pointy_GridToPosition( float size, HexGridCoord coord )
{
	Vector2 pos;

	pos.x = size * ( ( ( SQRT_THREE / 2.0f ) * coord.r ) + ( SQRT_THREE * coord.q ) );
	pos.y = size * ( ( 3.0f / 2.0f ) * coord.r );

	return pos;
}

HexGridCoord hex_Pointy_PositionToGrid( float size, Vector2 pos )
{
	float q = ( ( ( SQRT_THREE / 3.0f ) * pos.x ) - ( ( 1.0f / 3.0f ) * pos.y ) ) / size;
	float r = ( ( 2.0f / 3.0f ) * pos.y ) / size;

	return roundHexCoord( q, r );
}

static HexGridCoord neighborOffets[] = {
	{ 0, -1 }, { 1, -1 }, { 1, 0 }, { 0, 1 }, { -1, 1 }, { -1, 0 }
};

// For flat topped the neighbor at 0 will be at the top, for point topped the neighbor at 0 will be to the north-west
//  both will continue clockwise around the hex
// You can use the HexDirection_Flat and HexDirection_Pointy enumerations to more easily access the one you want
HexGridCoord hex_GetNeighbor( HexGridCoord baseCoord, int neighbor )
{
	assert( neighbor >= 0 );
	assert( neighbor < 6 );

	baseCoord.q += neighborOffets[neighbor].q;
	baseCoord.r += neighborOffets[neighbor].r;

	return baseCoord;
}

// Returns the distance in steps from the two grid coordinates.
int32_t hex_Distance( HexGridCoord from, HexGridCoord to )
{
	int dist = ( abs( from.q - to.q ) + abs( from.q + from.r - to.q - to.r ) + abs( from.r - to.r ) ) / 2;
	return dist;
}

// Get all hex coords within range steps of base and put them into the stretchy buffer sbOutList
void hex_AllInRange( HexGridCoord base, int32_t range, HexGridCoord** sbOutList )
{
	assert( sbOutList != NULL );
	assert( range >= 0 );

	sb_Clear( *sbOutList );

	for( int32_t x = -range; x <= range; ++x ) {

		int32_t min = MAX( -range, -x - range );
		int32_t max = MIN( range, -x + range );
		for( int32_t y = min; y <= max; ++y ) {
			int32_t z = -x - y;

			HexGridCoord c;
			c.q = x;
			c.r = z;

			add( &base, &c, &c );

			sb_Push( ( *sbOutList ), c );
		}
	}
}

static HexGridCoord hexLerp( HexGridCoord from, HexGridCoord to, float t )
{
	return roundHexCoord( lerp( (float)from.q, (float)to.q, t ), lerp( (float)from.r, (float)to.r, t ) );
}

// Get all hex coords along a line between two hex coords and put them into the stretchy buffer sbOutList
void hex_AllInLine( HexGridCoord start, HexGridCoord end, HexGridCoord** sbOutList )
{
	assert( sbOutList != NULL );

	int32_t dist = hex_Distance( start, end );

	for( int32_t i = 0; i <= dist; ++i ) {
		float t = ( 1.0f / dist ) * i;
		HexGridCoord c = hexLerp( start, end, t );
		sb_Push( ( *sbOutList ), c );
	}
}

// Get all hex coords that are a certain range from the center, where range > 0, and puts them into the stretchy buffer sbOutList
void hex_Ring( HexGridCoord center, int32_t range, HexGridCoord** sbOutList )
{
	assert( sbOutList != NULL );
	assert( range > 0 );

	// grab a direction and scale it by range, use the neighbors array
	//  start at neighbor 4 as that works best when starting with getting neighbor 0 in loop below
	HexGridCoord c = neighborOffets[4]; 
	c.q *= range;
	c.r *= range;
	add( &center, &c, &c );

	for( int i = 0; i < 6; ++i ) {
		for( int j = 0; j < range; ++j ) {
			sb_Push( ( *sbOutList ), c );
			c = hex_GetNeighbor( c, i );
		}
	}
}

//***************************************************************************
// Various functions to convert hex coordinates to grid indices
//  For these we'll assume the hex coord [0,0] corresponds to index 0
// hex <-> rect, pass in the width and height of the rectangular array
//  assuming information is densely packed and there is no empty space

/*
example
	 0,0  1,0  2,0  3,0  4,0  5,0  6,0
	 0,1  1,1  2,1  3,1  4,1  5,1  6,1
	-1,2  0,2  1,2  2,2  3,2  4,2  5,2
	-1,3  0,3  1,3  2,3  3,3  4,3  5,3
	-2,4 -1,4  0,4  1,4  2,4  3,4  4,4
	-2,5 -1,5  0,5  1,5  2,5  3,5  4,5
	-3,6 -2,6 -1,6  0,6  1,6  2,6  3,6

	0  1  2  3  4  5
	6  7  8  9  10 11
	12 13 14 15 16 17
	18 19 20 21 22 23
	24 25 26 27 28 29
	30 31 32 33 34 35
	*/

//#error get test rendering screen up and working, just render a 7x7 rectangle
bool hex_CoordInRect( HexGridCoord coord, int32_t width, int32_t height )
{
	assert( width > 0 );
	assert( height > 0 );

	if( ( coord.r < 0 ) || ( coord.r >= height ) ) return false;

	int minQ = -( coord.r / 2 );
	int maxQ = minQ + width;

	if( ( coord.q < minQ ) || ( coord.q >= maxQ ) ) return false;

	return true;
}

uint32_t hex_CoordToRectIndex( HexGridCoord coord, int32_t width, int32_t height )
{
	assert( width > 0 );
	assert( height > 0 );

	int qAdjustment = coord.r / 2;
	int adjQ = coord.q + qAdjustment;

	return adjQ + ( coord.r * width );
}

HexGridCoord hex_RectIndexToCoord( uint32_t index, int32_t width, int32_t height )
{
	assert( width > 0 );
	assert( height > 0 );

	HexGridCoord coord;

	coord.r = index / width;
	int qAdjustment = coord.r / 2;

	coord.q = ( index % width ) - qAdjustment;

	return coord;
}