#include <math.h>
#include <assert.h>
#include "../System/platformLog.h"
#include "vector3.h"

// component-wise operations
Vector3* vec3_Add( const Vector3* v1, const Vector3* v2, Vector3* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] + v2->v[0];
	out->v[1] = v1->v[1] + v2->v[1];
	out->v[2] = v1->v[2] + v2->v[2];

	return out;
}

Vector3* vec3_Subtract( const Vector3* v1, const Vector3* v2, Vector3* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] - v2->v[0];
	out->v[1] = v1->v[1] - v2->v[1];
	out->v[2] = v1->v[2] - v2->v[2];

	return out;
}

Vector3* vec3_HadamardProd( const Vector3* v1, const Vector3* v2, Vector3* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] * v2->v[0];
	out->v[1] = v1->v[1] * v2->v[1];
	out->v[2] = v1->v[2] * v2->v[2];

	return out;
}

Vector3* vec3_Divide( const Vector3* v1, const Vector3* v2, Vector3* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = v1->v[0] / v2->v[0];
	out->v[1] = v1->v[1] / v2->v[1];
	out->v[2] = v1->v[2] / v2->v[2];

	return out;
}

Vector3* vec3_Scale( const Vector3* vec, float scalar, Vector3* out )
{
	assert( vec != NULL );
	assert( out != NULL );

	out->v[0] = vec->v[0] * scalar;
	out->v[1] = vec->v[1] * scalar;
	out->v[2] = vec->v[2] * scalar;

	return out;
}

Vector3* vec3_AddScaled( const Vector3* base, const Vector3* scaled, const float scalar, Vector3* out )
{
	assert( base != NULL );
	assert( scaled != NULL );
	assert( out != NULL );

	out->v[0] = base->v[0] + ( scalar * scaled->v[0] );
	out->v[1] = base->v[1] + ( scalar * scaled->v[1] );
	out->v[2] = base->v[2] + ( scalar * scaled->v[2] );

	return out;
}

Vector3* vec3_Lerp( const Vector3* start, const Vector3* end, float t, Vector3* out )
{
	assert( start != NULL );
	assert( end != NULL );
	assert( out != NULL );

	float oneMinT = 1.0f - t;

	out->v[0] = ( oneMinT * start->v[0] ) + ( t * end->v[0] );
	out->v[1] = ( oneMinT * start->v[1] ) + ( t * end->v[1] );
	out->v[2] = ( oneMinT * start->v[2] ) + ( t * end->v[2] );

	return out;
}

// other operations
float vec3_DotProd( const Vector3* v1, const Vector3* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	return ( ( v1->v[0] * v2->v[0] ) + ( v1->v[1] * v2->v[1] ) + ( v1->v[2] * v2->v[2] ) );
}

Vector3* vec3_CrossProd( const Vector3* v1, const Vector3* v2, Vector3* out )
{
	assert( v1 != NULL );
	assert( v2 != NULL );
	assert( out != NULL );

	out->v[0] = ( v1->v[1] * v2->v[2] ) - ( v1->v[2] * v2->v[1] );
	out->v[1] = ( v1->v[2] * v2->v[0] ) - ( v1->v[0] * v2->v[2] );
	out->v[2] = ( v1->v[0] * v2->v[1] ) - ( v1->v[1] * v2->v[0] );

	return out;
}

float vec3_Mag( const Vector3* vec )
{
	return sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) + ( vec->v[2] * vec->v[2] ) );
}

float vec3_MagSqrd( const Vector3* vec )
{
	return ( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) + ( vec->v[2] * vec->v[2] ) );
}

float vec3_Dist( const Vector3* v1, const Vector3* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	Vector3 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];
	diff.v[2] = v1->v[2] - v2->v[2];

	return sqrtf( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) + ( diff.v[2] * diff.v[2] ) );
}

float vec3_DistSqrd( const Vector3* v1, const Vector3* v2 )
{
	assert( v1 != NULL );
	assert( v2 != NULL );

	Vector3 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];
	diff.v[2] = v1->v[2] - v2->v[2];

	return ( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) + ( diff.v[2] * diff.v[2] ) );
}

float vec3_Normalize( Vector3* vec )
{
	assert( vec != NULL );

	float mag = sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) + ( vec->v[2] * vec->v[2] ) );

	vec->v[0] /= mag;
	vec->v[1] /= mag;
	vec->v[2] /= mag;

	return mag;
}

Vector3* vec3_ProjOnto( const Vector3* vec, const Vector3* onto, Vector3* out )
{
	assert( vec != NULL );
	assert( onto != NULL );
	assert( out != NULL );

	float mult;

	mult = ( ( vec->v[0] * onto->v[0] ) + ( vec->v[1] * onto->v[1] ) + ( vec->v[2] * onto->v[2] ) ) / ( ( onto->v[0] * onto->v[0] ) + ( onto->v[1] * onto->v[1] ) + ( onto->v[2] * onto->v[2] ) );

	out->v[0] = onto->v[0] * mult;
	out->v[1] = onto->v[1] * mult;
	out->v[2] = onto->v[2] * mult;

	return out;
}

Vector3* vec3_Perpindicular( const Vector3* vec, const Vector3* ref, Vector3* out )
{
	assert( vec != NULL );
	assert( ref != NULL );
	assert( out != NULL );

	float mult;

	mult = ( ( vec->v[0] * ref->v[0] ) + ( vec->v[1] * ref->v[1] ) + ( vec->v[2] * ref->v[2] ) ) / ( ( ref->v[0] * ref->v[0] ) + ( ref->v[1] * ref->v[1] ) + ( ref->v[2] * ref->v[2] ) );

	out->v[0] = vec->v[0] - ( ref->v[0] * mult );
	out->v[1] = vec->v[1] - ( ref->v[1] * mult );
	out->v[2] = vec->v[2] - ( ref->v[2] * mult );

	return out;
}

Vector3 vec3( float x, float y, float z )
{
	Vector3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void vec3_Dump( const Vector3* vec, const char* extra )
{
	assert( vec != NULL );
	llog( LOG_DEBUG,  "%s = %3.3f %3.3f %3.3f\n", extra == NULL ? "v3" : extra, vec->v[0], vec->v[1], vec->v[2] );
}