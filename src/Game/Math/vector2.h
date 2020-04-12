/* inclusion guard */
#ifndef VECTOR_2_H
#define VECTOR_2_H

#include <stdbool.h>

typedef struct {
	union {
		struct {
			float x;
			float y;
		};
		struct {
			float s;
			float t;
		};
		struct {
			float w;
			float h;
		};
		struct {
			float fwd;
			float side;
		};
		float v[2];
	};
} Vector2;

static const Vector2 VEC2_ZERO = { 0.0f, 0.0f };
static const Vector2 VEC2_ONE = { 1.0f, 1.0f };
static const Vector2 VEC2_UP = { 0.0f, -1.0f };
static const Vector2 VEC2_DOWN = { 0.0f, 1.0f };
static const Vector2 VEC2_LEFT = { -1.0f, 0.0f };
static const Vector2 VEC2_RIGHT = { 1.0f, 0.0f };

Vector2 vec2( float x, float y );
Vector2* vec2_Add( const Vector2* v1, const Vector2* v2, Vector2* out );
Vector2* vec2_Subtract( const Vector2* v1, const Vector2* v2, Vector2* out );
Vector2* vec2_HadamardProd( const Vector2* v1, const Vector2* v2, Vector2* out );
Vector2* vec2_Scale( const Vector2* vec, const float scalar, Vector2* out );
Vector2* vec2_AddScaled( const Vector2* base, const Vector2* scaled, float scale, Vector2* out );
Vector2* vec2_Lerp( const Vector2* start, const Vector2* end, float t, Vector2* out );

float vec2_DotProduct( const Vector2* v1, const Vector2* v2 );

/* 2d cross product, returns the magnitude of the vector if we used the 3d cross product
on the 3d equivilant of the vectors. Absolute value will be the size of the parallelogram
described by the two vectors. */
float vec2_CrossProduct( const Vector2* v1, const Vector2* v2 );

float vec2_Mag( const Vector2* vec );
float vec2_MagSqrd( const Vector2* vec );
float vec2_Dist( const Vector2* v1, const Vector2* v2 );
float vec2_DistSqrd( const Vector2* v1, const Vector2* v2 );
float vec2_Normalize( Vector2* vec );

Vector2* vec2_NormalFromRot( float rotRad, Vector2* out );
Vector2* vec2_FromPolar( float rotRad, float magnitude, Vector2* out );
float vec2_RotationRadians( const Vector2* v );

Vector2* vec2_NormalFromSpineRot( float rotRad, Vector2* out );
float vec2_SpineRotationRadians( const Vector2* v );

Vector2* vec2_ProjOnto( const Vector2* vec, const Vector2* onto, Vector2* out );

Vector2* vec2_PerpRight( const Vector2* v, Vector2* out );
Vector2* vec2_PerpLeft( const Vector2* v, Vector2* out );

bool vec2_Comp( const Vector2* lhs, const Vector2* rhs );

void vec2_Dump( const Vector2* vec, const char* extra );

#endif /* end inclusion guard */
