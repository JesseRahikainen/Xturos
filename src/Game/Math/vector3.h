#ifndef VECTOR_3_H
#define VECTOR_3_H

typedef struct {
	union {
		struct {
			float x;
			float y;
			float z;
		};
		float v[3];
	};
} Vector3;

static const Vector3 VEC3_ZERO = { 0.0f, 0.0f, 0.0f };

// component-wise operations
Vector3* vec3_Add( const Vector3* v1, const Vector3* v2, Vector3* out );
Vector3* vec3_Subtract( const Vector3* v1, const Vector3* vec2, Vector3* out );
Vector3* vec3_HadamardProd( const Vector3* v1, const Vector3* vec2, Vector3* out );
Vector3* vec3_Divide( const Vector3* v1, const Vector3* vec2, Vector3* out );
Vector3* vec3_Scale( const Vector3* v, float scalar, Vector3* out );
Vector3* vec3_AddScaled( const Vector3* base, const Vector3* scaled, const float scalar, Vector3* out );
Vector3* vec3_Lerp( const Vector3* start, const Vector3* end, float t, Vector3* out );

// other operations
float vec3_DotProd( const Vector3* v1, const Vector3* v2 );

Vector3* vec3_CrossProd( const Vector3* v1, const Vector3* v2, Vector3* out );

float vec3_Mag( const Vector3* v );
float vec3_MagSqrd( const Vector3* v );

float vec3_Dist( const Vector3* v1, const Vector3* v2 );
float vec3_DistSqrd( const Vector3* v1, const Vector3* v2 );

float vec3_Normalize( Vector3* v );

Vector3* vec3_ProjOnto( const Vector3* vec, const Vector3* onto, Vector3* out );

Vector3* vec3_Perpindicular( const Vector3* vec, const Vector3* ref, Vector3* out );

Vector3 vec3( float x, float y, float z );

void vec3_Dump( const Vector3* vec, const char* extra );

#endif // inclusion guard