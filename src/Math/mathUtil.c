#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "mathUtil.h"

int isPowerOfTwo( int x )
{
	return ( ( x != 0 ) && !( x & ( x - 1 ) ) );
}

float lerp( float from, float to, float t )
{
	return ( from + ( ( to - from ) * clamp( 0.0f, 1.0f, t ) ) );
}

float radianRotLerp( float from, float to, float t )
{
	// always use the shortest angle
	float diff = to - from;
	float sgn = sign( diff );
	diff = fabsf( diff );
	
	if( diff > M_PI_F ) {
		diff = ( 2.0f * M_PI_F ) - diff;
		sgn = -sgn;
	}
	
	return ( from + ( diff * sgn * clamp( 0.0f, 1.0f, t ) ) );
}

float degreeRotLerp( float from, float to, float t )
{
	// always use the shortest angle
	float diff = to - from;
	float sgn = sign( diff );
	diff = fabsf( diff );
	
	if( diff > 180.0f ) {
		diff = 360.0f - diff;
		sgn = -sgn;
	}
	
	return ( from + ( diff * sgn * clamp( 0.0f, 1.0f, t ) ) );
}

float degreeRotDiff( float from, float to )
{
	assert( from >= -180.0f );
	assert( from <= 180.0f );
	assert( to >= -180.0f );
	assert( to <= 180.0f );
	float diff = to - from;

	if( diff > 180.0f ) {
		diff -= 360.0f;
	}

	if( diff < -180.0f ) {
		diff += 360.0f;
	}

	return diff;
}

float degreeRotWrap( float deg )
{
	while( deg > 180.0f ) {
		deg -= 360.0f;
	}

	while( deg < -180.0f ) {
		deg += 360.0f;
	}

	return deg;
}

float spineDegRotToEngineDegRot( float spineDeg )
{
	float rot = spineDeg + 90.0f;
	return rot;
}

float engineDegRotToSpineDegRot( float engineDeg )
{
	float rot = 0.0f;

	return rot;
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

float jerkLerp( float t )
{
	t = clamp( 0.0f, 1.0f, t );
	return ( 1.0f - ( ( 1.0f - t ) * ( 1.0f - t ) ) );
}