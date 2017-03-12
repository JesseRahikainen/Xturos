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

float randFloatVar( float mid, float var )
{
	assert( var >= 0.0f );
	return randFloat( mid - var, mid + var );
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

// Real-Time Collision Detection, pg 128
void closestPtToSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos, Vector2* outPos, float* outParam )
{
	Vector2 line;
	vec2_Subtract( segOne, segTwo, &line );

	// project pos onto the line
	Vector2 toPos;
	vec2_Subtract( segOne, pos, &toPos );
	float t = vec2_DotProduct( &toPos, &line ) / vec2_DotProduct( &line, &line );

	// if outside the segment clamp t to the nearest endpoint
	if( t < 0.0f ) t = 0.0f;
	if( t > 1.0f ) t = 1.0f;

	vec2_Lerp( segOne, segTwo, t, outPos );
	(*outParam) = t;
}

// Real-Time Collision Detection, pg 130
float sqDistPointSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos )
{
	Vector2 twoMinOne;
	Vector2 posMinOne;
	Vector2 posMinTwo;

	vec2_Subtract( segTwo, segOne, &twoMinOne );
	vec2_Subtract( pos, segOne, &posMinOne );
	vec2_Subtract( pos, segTwo, &posMinTwo );

	float e = vec2_DotProduct( &posMinOne, &twoMinOne );

	// handles cases where pos projects outside the segment
	if( e <= 0.0f )  {
		return vec2_DotProduct( &posMinOne, &posMinOne );
	}

	float f = vec2_DotProduct( &twoMinOne, &twoMinOne );
	if( e >= f ) {
		return vec2_DotProduct( &posMinTwo, &posMinTwo );
	}

	return vec2_DotProduct( &posMinOne, &posMinOne ) - ( ( e * e ) / f );
}