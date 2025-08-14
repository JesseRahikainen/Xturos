#include <stdlib.h>
#include <SDL3/SDL_assert.h>
#include <math.h>
#include "mathUtil.h"
#include "Utils/helpers.h"

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

float radianRotDiff( float from, float to )
{
	from = radianRotWrap( from );
	to = radianRotWrap( to );

	float diff = fmodf( ( to - from + M_PI_F ), M_TWO_PI_F ) - M_PI_F;

	if( diff > M_PI_F ) {
		diff -= M_TWO_PI_F;
	}

	while( diff < -M_PI_F ) {
		diff += M_TWO_PI_F;
	}

	return diff;
}

float degreeRotDiff( float from, float to )
{
	from = degreeRotWrap( from );
	to = degreeRotWrap( to );
	float diff = fmodf( ( to - from + 180.0f ), 360.0f ) - 180.0f;

	if( diff > 180.0f ) {
		diff -= 360.0f;
	}

	if( diff < -180.0f ) {
		diff += 360.0f;
	}

	return diff;
}

float radianRotWrap( float rad )
{
	while( rad > M_PI_F ) {
		rad -= M_TWO_PI_F;
	}

	while( rad < -M_PI_F ) {
		rad += M_TWO_PI_F;
	}

	return rad;
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

float hermiteBlend( float t )
{
	t = clamp( 0.0f, 1.0f, t );
	// 3t^2 - 2t^3
	return ( ( 3.0f * t * t ) - ( 2.0f * t * t * t ) );
}

// smoother blend (first and second derivatives have a slope of 0)
//  gotten from Perlin's paper on simplex noise, hence the name
float perlinBlend( float t )
{
	t = clamp( 0.0f, 1.0f, t );
	// 6t^5 - 15t^4 + 10t^3
	return ( ( 6.0f * t * t * t * t * t ) - ( 15.0f * t * t * t * t ) + ( 10.0f * t * t * t ) );
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
	SDL_assert( min <= max );

	if( val < min ) {
		return min;
	}

	if( val > max ) {
		return max;
	}

	return val;
}

float clamp01( float val )
{
	return MIN( 1.0f, MAX( val, 0.0f ) );
}

float randFloat( float min, float max )
{
	SDL_assert( min <= max );
	return min + ( ( ( (float)rand( ) / (float)RAND_MAX ) ) * ( max - min ) );
}

float randFloatVar( float mid, float var )
{
	SDL_assert( var >= 0.0f );
	return randFloat( mid - var, mid + var );
}

float sign( float val )
{
	return ( ( val >= 0.0f ) ? 1.0f : -1.0f );
}

int signi( int val )
{
	return ( ( val >= 0 ) ? 1 : -1 );
}

Vector3* vec2ToVec3( const Vector2* vec2, float z, Vector3* out )
{
	SDL_assert( vec2 != NULL );
	SDL_assert( out != NULL );

	out->v[0] = vec2->v[0];
	out->v[1] = vec2->v[1];
	out->v[2] = z;

	return out;
}

// maps [0,1] the an arc starting and ending at y = 0 and the peak at 0.5 where y = 1
float jerkLerp( float t )
{
	t = clamp( 0.0f, 1.0f, t );
	return ( 1.0f - ( ( 1.0f - t ) * ( 1.0f - t ) ) );
}

float remap( float origMin, float origMax, float val, float newMin, float newMax )
{
	return lerp( newMin, newMax, inverseLerp( origMin, origMax, val ) );
}

int wrapi( int val, int min, int max )
{
	SDL_assert( min <= max );

	int range = ( max - min ) + 1;
	int offset = val - min;

	while( offset >= range ) offset -= range;
	while( offset < 0 ) offset += range;

	return min + offset;
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

float signed2DTriArea( const Vector2* a, const Vector2* b, const Vector2* c )
{
	return ( ( a->x - c->x ) * ( b->y - c->y ) - ( a->y - c->y ) * ( b->x - c->x ) );
}

int digitsInI32( int32_t num )
{
	if( num < 0 ) num = ( num == INT32_MIN ) ? INT32_MAX : -num;
	return digitsInU32( (uint32_t)num );
}

int digitsInU32( uint32_t num )
{
	if( num < 10 ) return 1;
	if( num < 100 ) return 2;
	if( num < 1000 ) return 3;
	if( num < 10000 ) return 4;
	if( num < 100000 ) return 5;
	if( num < 1000000 ) return 6;
	if( num < 10000000 ) return 7;
	if( num < 100000000 ) return 8;
	if( num < 1000000000 ) return 9;
	return 10;
}

int32_t divisorForDigitExtractionI32( int32_t num )
{
	if( num < 0 ) num = ( num == INT32_MIN ) ? INT32_MAX : -num;
	return (int32_t)divisorForDigitExtractionU32( (uint32_t)num );
}

uint32_t divisorForDigitExtractionU32( uint32_t num )
{
	if( num < 10 ) return 1;
	if( num < 100 ) return 10;
	if( num < 1000 ) return 100;
	if( num < 10000 ) return 1000;
	if( num < 100000 ) return 10000;
	if( num < 1000000 ) return 100000;
	if( num < 10000000 ) return 1000000;
	if( num < 100000000 ) return 10000000;
	if( num < 1000000000 ) return 100000000;
	return 1000000000;
}

// finds the squared distance from the point pos to the line segment defined by lineA to lineB
float sqrdDistToSegment( Vector2* pos, Vector2* lineA, Vector2* lineB )
{
	SDL_assert( pos != NULL );
	SDL_assert( lineA != NULL );
	SDL_assert( lineB != NULL );

	Vector2 ab;
	vec2_Subtract( lineB, lineA, &ab );

	Vector2 ap;
	vec2_Subtract( pos, lineA, &ap );

	Vector2 bp;
	vec2_Subtract( pos, lineB, &bp );

	float e = vec2_DotProduct( &ap, &ab );

	if( e <= 0.0f ) {
		return vec2_DotProduct( &ap, &ap );
	}

	float f = vec2_DotProduct( &ab, &ab );
	if( e >= f ) {
		return vec2_DotProduct( &bp, &bp );
	}

	return vec2_DotProduct( &ap, &ap ) - ( ( e * e ) / f );
}

// returns if pos is within lineWidth distance from all the points polygon
bool isPointOnPolygon( Vector2* pos, Vector2* polygon, size_t numPoints, float lineWidth )
{
	float halfWidthSqrd = lineWidth / 2.0f;
	halfWidthSqrd *= halfWidthSqrd;

	for( size_t i = 0; i < numPoints; ++i ) {
		if( sqrdDistToSegment( pos, polygon + i, polygon + ( ( i + 1 ) % numPoints ) ) <= halfWidthSqrd ) {
			return true;
		}
	}

	return false;
}

void fitRatioInsideSpace( float ratio, Vector2* fitterSize, Vector2* outSize )
{
	SDL_assert( fitterSize != NULL );
	SDL_assert( outSize != NULL );

	float fitterRatio = fitterSize->w / fitterSize->h;

	if( fitterRatio <= ratio ) {
		// fitter will be taller than fitted
		outSize->w = fitterSize->w;
		outSize->h = fitterSize->w / ratio;
	} else {
		// fitter will be wider than fitted
		outSize->h = fitterSize->h;
		outSize->w = fitterSize->h * ratio;
	}
}

void fitRatioInsideRect( float ratio, Vector2* fitterMins, Vector2* fitterMaxes, Vector2* outMins, Vector2* outMaxes )
{
	SDL_assert( fitterMins != NULL );
	SDL_assert( fitterMaxes != NULL );
	SDL_assert( outMins != NULL );
	SDL_assert( outMaxes != NULL );

	// first find ratio of fitter area
	Vector2 fitterSize;
	vec2_Subtract( fitterMaxes, fitterMins, &fitterSize );
	float fitterRatio = fitterSize.w / fitterSize.h;

	if( fitterRatio <= ratio ) {
		// fitter will be taller than fitted, centered along y-axis and matching x-axis
		float centerY = ( fitterMins->y + fitterMaxes->y ) / 2.0f;
		outMins->x = fitterMins->x;
		outMaxes->x = fitterMaxes->x;
		float width = outMaxes->x - outMins->x;
		float halfHeight = ( width / ratio ) / 2.0f;
		outMins->y = centerY - halfHeight;
		outMaxes->y = centerY + halfHeight;
	} else {
		// fitter will be wider than fitted, centered along x-axis and matching y-axis
		float centerX = ( fitterMins->x + fitterMaxes->x ) / 2.0f;
		outMins->y = fitterMins->y;
		outMaxes->y = fitterMaxes->y;
		float height = outMaxes->y - outMins->y;
		float halfWidth = ( height * ratio ) / 2.0f;
		outMins->x = centerX - halfWidth;
		outMaxes->x = centerX + halfWidth;
	}
}

float exponentialSmoothing( float current, float target, float speed, float dt )
{
	return current + ( target - current ) * ( 1.0f - SDL_expf( -speed * dt ) );
}

Vector2* exponentialSmoothingV2( const Vector2* current, const Vector2* target, float speed, float dt, Vector2* out )
{
	ASSERT_AND_IF_NOT( current != NULL ) return out;
	ASSERT_AND_IF_NOT( target != NULL ) return out;
	ASSERT_AND_IF_NOT( out != NULL ) return out;

	float scalar = ( 1.0f - SDL_expf( -speed * dt ) );
	vec2_Subtract( target, current, out );
	vec2_Scale( out, scalar, out );

	return out;
}

// simple cubic bezier
Vector2 cubicBezier( const Vector2 zero, const Vector2 one, const Vector2 two, const Vector2 three, float u )
{
	float oneMinU = 1.0f - u;

	Vector2 result = VEC2_ZERO;
	vec2_AddScaled( &result, &zero, oneMinU * oneMinU * oneMinU, &result );
	vec2_AddScaled( &result, &one, 3 * u * ( oneMinU * oneMinU ), &result );
	vec2_AddScaled( &result, &two, 3 * ( u * u ) * oneMinU, &result );
	vec2_AddScaled( &result, &three, u * u * u, &result );

	return result;
}

// generates a rectangle of the passed in ratio that goes around the fitter rectangle, the axis that isn't tight to the fitter will be centered around the fitter
void envelopeRect( float widthHeightRatio, const Vector2* fitterMins, const Vector2* fitterMaxes, Vector2* outMins, Vector2* outMaxes )
{
	ASSERT_AND_IF_NOT( fitterMins != NULL ) return;
	ASSERT_AND_IF_NOT( fitterMaxes != NULL ) return;
	ASSERT_AND_IF_NOT( outMins != NULL ) return;
	ASSERT_AND_IF_NOT( outMaxes != NULL ) return;
	ASSERT_AND_IF_NOT( fitterMins->x <= fitterMaxes->x ) return;
	ASSERT_AND_IF_NOT( fitterMins->y <= fitterMaxes->y ) return;

	float fitterWidth = fitterMaxes->x - fitterMins->x;
	float fitterHeight = fitterMaxes->y - fitterMins->x;

	float fitterRatio = fitterWidth / fitterHeight;

	if( widthHeightRatio > fitterRatio ) {
		// out rect will be wider than fitter, use the vertical part of the fitter
		outMins->y = fitterMins->y;
		outMaxes->y = fitterMaxes->y;
		float halfOutWidth = ( fitterHeight * widthHeightRatio ) / 2.0f;
		float center = ( fitterMins->x + fitterMaxes->x ) / 2.0f;
		outMins->x = center - halfOutWidth;
		outMaxes->x = center + halfOutWidth;
	} else {
		// out rect will be taller than fitter, use the horizontal part of the fitter
		outMins->x = fitterMins->x;
		outMaxes->x = fitterMaxes->x;
		float halfOutHeight = ( widthHeightRatio / fitterWidth ) / 2.0f;
		float center = ( fitterMins->y + fitterMaxes->y ) / 2.0f;
		outMins->y = center - halfOutHeight;
		outMaxes->y = center + halfOutHeight;
	}
}