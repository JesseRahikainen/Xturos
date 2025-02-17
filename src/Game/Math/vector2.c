#include "vector2.h"
#include "mathUtil.h"
#include <math.h>
#include <string.h>
#include <SDL3/SDL_assert.h>
#include "System/platformLog.h"

Vector2 vec2( float x, float y )
{
	Vector2 v;
	v.x = x;
	v.y = y;

	return v;
}

Vector2* vec2_Add( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );
	SDL_assert( out != NULL );

	out->v[0] = v1->v[0] + v2->v[0];
	out->v[1] = v1->v[1] + v2->v[1];

	return out;
}

Vector2* vec2_Subtract( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );
	SDL_assert( out != NULL );

	out->v[0] = v1->v[0] - v2->v[0];
	out->v[1] = v1->v[1] - v2->v[1];

	return out;
}

Vector2* vec2_HadamardProd( const Vector2* v1, const Vector2* v2, Vector2* out )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );
	SDL_assert( out != NULL );

	out->v[0] = v1->v[0] * v2->v[0];
	out->v[1] = v1->v[1] * v2->v[1];

	return out;
}

Vector2* vec2_Scale( const Vector2* vec, const float scalar, Vector2* out )
{
	SDL_assert( vec != NULL );
	SDL_assert( out != NULL );

	out->v[0] = vec->v[0] * scalar;
	out->v[1] = vec->v[1] * scalar;

	return out;
}

Vector2* vec2_AddScaled( const Vector2* base, const Vector2* scaled, float scale, Vector2* out )
{
	SDL_assert( base != NULL );
	SDL_assert( scaled != NULL );
	SDL_assert( out != NULL );

	out->v[0] = base->v[0] + ( scaled->v[0] * scale );
	out->v[1] = base->v[1] + ( scaled->v[1] * scale );

	return out;
}

Vector2* vec2_Lerp( const Vector2* start, const Vector2* end, float t, Vector2* out )
{
	SDL_assert( start != NULL );
	SDL_assert( end != NULL );
	SDL_assert( out != NULL );

	out->v[0] = start->v[0] + ( ( end->v[0] - start->v[0] ) * t );
	out->v[1] = start->v[1] + ( ( end->v[1] - start->v[1] ) * t );

	return out;
}

float vec2_DotProduct( const Vector2* v1, const Vector2* v2 )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );

	return ( ( v1->v[0] * v2->v[0] ) + ( v1->v[1] * v2->v[1] ) );
}

/* 2d cross product, returns the magnitude of the vector if we used the 3d cross product
on the 3d equivilant of the vectors. Absolute value will be the size of the parallelogram
described by the two vectors. */
float vec2_CrossProduct( const Vector2* v1, const Vector2* v2 )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );

	return ( ( v1->v[0] * v2->v[1] ) - ( v1->v[1] * v2->v[0] ) );
}

float vec2_Mag( const Vector2* vec )
{
	SDL_assert( vec != NULL );

	return sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );
}

float vec2_MagSqrd( const Vector2* vec )
{
	SDL_assert( vec != NULL );

	return ( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );
}

float vec2_Dist( const Vector2* v1, const Vector2* v2 )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );

	Vector2 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];

	return sqrtf( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) );
}

float vec2_DistSqrd( const Vector2* v1, const Vector2* v2 )
{
	SDL_assert( v1 != NULL );
	SDL_assert( v2 != NULL );

	Vector2 diff;
	diff.v[0] = v1->v[0] - v2->v[0];
	diff.v[1] = v1->v[1] - v2->v[1];

	return ( ( diff.v[0] * diff.v[0] ) + ( diff.v[1] * diff.v[1] ) );
}

float vec2_Normalize( Vector2* vec )
{
	SDL_assert( vec != NULL );

	float mag = sqrtf( ( vec->v[0] * vec->v[0] ) + ( vec->v[1] * vec->v[1] ) );

	if( mag != 0.0f ) {
		vec->v[0] /= mag;
		vec->v[1] /= mag;
	}

	return mag;
}

Vector2* vec2_NormalFromRot( float rotRad, Vector2* out )
{
	SDL_assert( out != NULL );

	out->x = sinf( rotRad );
	out->y = -cosf( rotRad );

	return out;
}

Vector2* vec2_FromPolar( float rotRad, float magnitude, Vector2* out )
{
	SDL_assert( out != NULL );

	out->x = sinf( rotRad ) * magnitude;
	out->y = -cosf( rotRad ) * magnitude;

	return out;
}

float vec2_RotationRadians( const Vector2* v )
{
	SDL_assert( v != NULL );

	float dir = atan2f( -v->y, -v->x ) - ( M_PI_F / 2.0f );
	return dir;
}

Vector2* vec2_ProjOnto( const Vector2* vec, const Vector2* onto, Vector2* out )
{
	SDL_assert( vec != NULL );
	SDL_assert( onto != NULL );
	SDL_assert( out != NULL );

	float mult;

	mult = ( ( vec->v[0] * onto->v[0] ) + ( vec->v[1] * onto->v[1] ) ) / ( ( onto->v[0] * onto->v[0] ) + ( onto->v[1] * onto->v[1] ) );

	out->v[0] = onto->v[0] * mult;
	out->v[1] = onto->v[1] * mult;

	return out;
}

Vector2* vec2_PerpRight( const Vector2* v, Vector2* out )
{
	SDL_assert( v != NULL );
	SDL_assert( out != NULL );
	SDL_assert( v != out );

	out->x = -( v->y );
	out->y = v->x;

	return out;
}

Vector2* vec2_PerpLeft( const Vector2* v, Vector2* out )
{
	SDL_assert( v != NULL );
	SDL_assert( out != NULL );
	SDL_assert( v != out );

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
	SDL_assert( vec != NULL );
	llog( LOG_DEBUG, "%s = %3.3f %3.3f\n", extra == NULL ? "v2" : extra, vec->v[0], vec->v[1] );
}

bool vec2_Serialize( cmp_ctx_t* cmp, const Vector2* vec )
{
	SDL_assert( vec != NULL );
	SDL_assert( cmp != NULL );

	if( !cmp_write_float( cmp, vec->x ) ) {
		llog( LOG_ERROR, "Unable to write x coordinate of Vector2." );
		return false;
	}

	if( !cmp_write_float( cmp, vec->y ) ) {
		llog( LOG_ERROR, "Unable to write y coordinate of Vector2." );
		return false;
	}

	return true;
}

bool vec2_Deserialize( cmp_ctx_t* cmp, Vector2* outVec )
{
	SDL_assert( outVec != NULL );
	SDL_assert( cmp != NULL );

	outVec->x = 0.0f;
	outVec->y = 0.0f;

	if( !cmp_read_float( cmp, &( outVec->x ) ) ) {
		llog( LOG_ERROR, "Unable to load x coordinate of Vector2." );
		return false;
	}

	if( !cmp_read_float( cmp, &( outVec->y ) ) ) {
		llog( LOG_ERROR, "Unable to load y coordinate of Vector2." );
		return false;
	}

	return true;
}