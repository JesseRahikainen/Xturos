#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include "vector2.h"
#include "vector3.h"
#include <stdint.h>
#include <stdbool.h>

#define M_PI_F 3.14159265359f
#define M_TWO_PI_F ( 2.0f * M_PI_F )

#define DEG_TO_RAD( d ) ( ( ( d ) * ( M_PI_F / 180.0f ) ) )
#define RAD_TO_DEG( d ) ( ( ( d ) * ( 180.0f / M_PI_F ) ) )

#define MIN( a, b ) ( ((a)>(b))?(b):(a) )
#define MAX( a, b ) ( ((a)>(b))?(a):(b) )

int isPowerOfTwo( int x );
float lerp( float from, float to, float t );

float radianRotLerp( float from, float to, float t );
float degreeRotLerp( float from, float to, float t );
float radianRotDiff( float from, float to );
float degreeRotDiff( float from, float to );
float radianRotWrap( float rad );
float degreeRotWrap( float deg );
float spineDegRotToEngineDegRot( float spineDeg );
float engineDegRotToSpineDegRot( float engineDeg );

uint8_t lerp_uint8_t( uint8_t from, uint8_t to, float t );
float inverseLerp( float from, float to, float val );
float clamp( float min, float max, float val );
float randFloat( float min, float max );
float randFloatVar( float mid, float var );
float sign( float val );
Vector3* vec2ToVec3( const Vector2* vec2, float z, Vector3* out );
float jerkLerp( float t );
float remap( float origMin, float origMax, float val, float newMin, float newMax );

void closestPtToSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos, Vector2* outPos, float* outParam );
float sqDistPointSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos );

float signed2DTriArea( const Vector2* a, const Vector2* b, const Vector2* c );

#define FLOAT_TOLERANCE 0.0001f
// used for testing floats within a certain tolerance, TODO: configurable tolerance?
#define FLT_EQ( f, t ) ( ( ( ( f ) - ( t ) ) <= FLOAT_TOLERANCE ) && ( ( f ) - ( t ) >= -FLOAT_TOLERANCE ) )
#define FLT_LE( f, t ) ( ( f ) - ( t ) <= FLOAT_TOLERANCE )
#define FLT_GE( f, t ) ( ( f ) - ( t ) >= -FLOAT_TOLERANCE )
#define FLT_LT( f, t ) ( ( f ) - ( t ) < FLOAT_TOLERANCE )
#define FLT_GT( f, t ) ( ( f ) - ( t ) > -FLOAT_TOLERANCE )
#define FLT_BETWEEN( f, low, high ) ( FLT_LE( f, high ) && FLT_GE( f, low ) )

// finds the squared distance from the point pos to the line segment defined by lineA to lineB
float sqrdDistToSegment( Vector2* pos, Vector2* lineA, Vector2* lineB );

// returns if pos is within lineWidth distance from all the points polygon
bool isPointOnPolygon( Vector2* pos, Vector2* polygon, size_t numPoints, float lineWidth );

#endif // inclusion guard