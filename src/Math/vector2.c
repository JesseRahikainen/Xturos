#include "vector2.h"
#include "mathUtil.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../System/platformLog.h"

Vector2 vec2( float x, float y )
{
	Vector2 v;
	v.x = x;
	v.y = y;

	return v;
}

Vector2* vec2_Add( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] + v2->v[0];
	out->v[1] = v1->v[1] + v2->v[1];

	return out;
}

Vector2* vec2_Subtract( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] - v2->v[0];
	out->v[1] = v1->v[1] - v2->v[1];

	return out;
}

Vector2* vec2_HadamardProd( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] * v2->v[0];
	out->v[1] = v1->v[1] * v2->v[1];

	return out;
}

Vector2* vec2_Scale( const Vector2* vec, const float scalar, Vector2* out )
{
	assert( vec != NULL );
	assert( out != NULL );

	out->v[0] = vec->v[0] * scalar;
	out->v[1] = vec->v[1] * scalar;

	return out;
}

Vector2* vec2_AddScaled( const Vector2* base, const Vector2* scaled, float scale, Vector2* out )
{
	assert( base != NULL );
	assert( scaled != NULL );
	assert( out != NULL );

	out->v[0] = base->v[0] + ( scaled->v[0] * scale );
	out->v[1] = base->v[1] + ( scaled->v[1] * scale );

	return out;
}

Vector2* vec2_Lerp( const Vector2* start, const Vector2* end, float t, Vector2* out )
{
	assert( start != NULL );
	assert( end != NULL );
	assert( out != NULL );

	out->v[0] = start->v[0] + ( ( end->v[0] - start->v[0] ) * t );
	out->v[1] = start->v[1] + ( ( end->v[1] - start->v[1] ) * t );

	return out;
}

float vec2_DotProduct( const Vector2* v1, const Vector2* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	return ( ( v1->v[0] * v2->v[0] ) + ( v1->v[1] * v2->v[1] ) );
}

/* 2d cross product, returns the magnitude of the vector if we used the 3d cross product
on the 3d equivilant of the vectors. Absolute value will be the size of the parallelogram
described by the two vectors. */
float vec2_CrossProduct( const Vector2* v1, const Vector2* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	return ( ( v1->v[0] * v2->v[1] ) - ( v1->v[1] * v2->v[0] ) );
}

float vec2_Mag( const Vector2* vec )
{
	assert( vec != NULL );

	return sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );
}

float vec2_MagSqrd( const Vector2* vec )
{
	assert( vec != NULL );

	return ( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );
}

float vec2_Dist( const Vector2* v1, const Vector2* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	Vector2 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];

	return sqrtf( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) );
}

float vec2_DistSqrd( const Vector2* v1, const Vector2* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	Vector2 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];

	return ( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) );
}

float vec2_Normalize( Vector2* vec )
{
	assert( vec != NULL );

	float mag = sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );

	if( mag != 0.0f ) {
		vec->v[0] /= mag;
		vec->v[1] /= mag;
	}

	return mag;
}

Vector2* vec2_NormalFromRot( float rotRad, Vector2* out )
{
	assert( out != NULL );

	out->x = sinf( rotRad );
	out->y = -cosf( rotRad );

	return out;
}

Vector2* vec2_FromPolar( float rotRad, float magnitude, Vector2* out )
{
	assert( out != NULL );

	out->x = sinf( rotRad ) * magnitude;
	out->y = -cosf( rotRad ) * magnitude;

	return out;
}

float vec2_RotationRadians( const Vector2* v )
{
	assert( v != NULL );

	float dir = atan2f( -v->y, -v->x ) - ( M_PI_F / 2.0f );
	return dir;
}

Vector2* vec2_NormalFromSpineRot( float rotRad, Vector2* out )
{
	assert( out != NULL );

	out->x = -sinf( rotRad );
	out->y = -cosf( rotRad );

	return out;
}

float vec2_SpineRotationRadians( const Vector2* v )
{
	assert( v != NULL );

	float dir = atan2f( v->y, -v->x ) + ( M_PI_F / 2.0f );
	return dir;
}

Vector2* vec2_ProjOnto( const Vector2* vec, const Vector2* onto, Vector2* out )
{
	assert( vec != NULL );
	assert( onto != NULL );
	assert( out != NULL );

	float mult;

	mult = ( ( vec->v[0] * onto->v[0] ) + ( vec->v[1] * onto->v[1] ) ) / ( ( onto->v[0] * onto->v[0] ) + ( onto->v[1] * onto->v[1] ) );

	out->v[0] = onto->v[0] * mult;
	out->v[1] = onto->v[1] * mult;

	return out;
}

Vector2* vec2_PerpRight( const Vector2* v, Vector2* out )
{
	assert( v != NULL );
	assert( out != NULL );

	out->x = -( v->y );
	out->y = v->x;

	return out;
}

Vector2* vec2_PerpLeft( const Vector2* v, Vector2* out )
{
	assert( v != NULL );
	assert( out != NULL );

	out->x = v->y;
	out->y = -( v->x );

	return out;
}

bool vec2_Comp( const Vector2* lhs, const Vector2* rhs )
{
	return ( FLT_EQ( lhs->x, rhs->x ) && FLT_EQ( lhs->y, rhs->y ) );
}

void vec2_Dump( const Vector2* vec, const char* extra )
{
	assert( vec != NULL );
	llog( LOG_DEBUG, "%s = %3.3f %3.3f\n", extra == NULL ? "v2" : extra, vec->v[0], vec->v[1] );
}