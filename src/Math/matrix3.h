#ifndef MATRIX_3_H
#define MATRIX_3_H

#include <stdbool.h>

#include "vector3.h"
#include "vector2.h"

/* Column-major 3x3 matrix
[ 0 3 6 ]
[ 1 4 7 ]
[ 2 5 8 ]

[ x.x y.x z.x ]
[ x.y y.y x.y ]
[ x.z y.z z.z ] */

typedef struct {
	float m[9];
} Matrix3;

static const Matrix3 IDENTITY_MATRIX_3 = {
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 1.0f
};

static const Matrix3 ZERO_MATRIX_3 = {
	0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f
};

Matrix3* mat3_Multiply( const Matrix3* m, const Matrix3* n, Matrix3* out );

Vector3* mat3_TransformVec3Dir( const Matrix3* m, const Vector3* v, Vector3* out );
void mat3_SetColumn( Matrix3* m, int column, const Vector3* col );
Vector3* mat3_GetColumn( const Matrix3* m, int column, Vector3* out );
Vector2* mat3_GetColumn_2( const Matrix3* m, int column, Vector2* out );
void mat3_SetPosition( Matrix3* m, const Vector2* pos );
Vector2* mat3_GetPosition( const Matrix3* m, Vector2* out ); 
Matrix3* mat3_SetRotation( float rot, Matrix3* out );

Matrix3* mat3_CreateTranslation( float fwd, float side, Matrix3* out );

Vector2* mat3_TransformVec2Dir( const Matrix3* m, const Vector2* v, Vector2* out );
Vector2* mat3_TransformVec2Pos( const Matrix3* m, const Vector2* v, Vector2* out );
Vector2* mat3_TransformVec2Pos_InPlace( const Matrix3* m, Vector2* v );

bool mat3_Inverse( const Matrix3* m, Matrix3* out );

#endif /* inclusion guard */