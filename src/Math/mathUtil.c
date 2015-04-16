#include <stdlib.h>
#include <assert.h>
#include "mathUtil.h"

int isPowerOfTwo( int x )
{
	return ( ( x != 0 ) && !( x & ( x - 1 ) ) );
}

float lerp( float from, float to, float t )
{
	return ( from + ( ( to - from ) * clamp( 0.0f, 1.0f, t ) ) );
}

uint8_t lerp_uint8_t( uint8_t from, uint8_t to, float t )
{
	return (uint8_t)lerp( (float)from, (float)to, t );
}

float inverseLerp( float from, float to, float val )
{
	if( from < to ) {
		return clamp( 0.0f, 1.0f, ( ( val - from ) / ( to - from ) ) );
	} else {
		return clamp( 0.0f, 1.0f, ( 1.0f - ( ( val - to ) / ( from - to ) ) ) );
	}
}

float clamp( float min, float max, float val )
{
	assert( min <= max );

	if( val < min ) {
		return min;
	}

	if( val > max ) {
		return max;
	}

	return val;
}

float randFloat( float min, float max )
{
	assert( min <= max );
	return min + ( ( ( (float)rand( ) / (float)RAND_MAX ) ) * ( max - min ) );
}

float sign( float val )
{
	return ( ( val >= 0.0f ) ? 1.0f : -1.0f );
}

Vector3* vec2ToVec3( const Vector2* vec2, float z, Vector3* out )
{
	assert( vec2 != NULL );
	assert( out != NULL );

	out->v[0] = vec2->v[0];
	out->v[1] = vec2->v[1];
	out->v[2] = z;

	return out;
}