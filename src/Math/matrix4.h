#ifndef MATRIX_4_H
#define MATRIX_4_H

#include <stdbool.h>

#include "vector3.h"
#include "mathUtil.h"

/* Column-major 4x4 matrix
  [ 0 4  8 12 ]
  [ 1 5  9 13 ]
  [ 2 6 10 14 ]
  [ 3 7 11 15 ]

  [ x.x y.x z.x t.x ]
  [ x.y y.y x.y t.y ]
  [ x.z y.z z.z t.z ]
  [  0   0   0   1  ] */

typedef struct{
	float m[16];
} Matrix4;

static const Matrix4 IDENTITY_MATRIX = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

static const Matrix4 ZERO_MATRIX = {
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f
};

// handles if out is m or n
Matrix4* mat4_Multiply( const Matrix4* m, const Matrix4* n, Matrix4* out );

Matrix4* mat4_CreateXRotation( float angleRad, Matrix4* out );
Matrix4* mat4_CreateYRotation( float angleRad, Matrix4* out );
Matrix4* mat4_CreateZRotation( float angleRad, Matrix4* out );
Matrix4* mat4_CreateTranslation( float x, float y, float z, Matrix4* out );
Matrix4* mat4_CreateTranslation_v( const Vector3* v, Matrix4* out );
Matrix4* mat4_CreateScale( float x, float y, float z, Matrix4* out );
Matrix4* mat4_CreateScale_v( const Vector3* v, Matrix4* out );

Matrix4* mat4_Scale( Matrix4* m, float x, float y, float z );
Matrix4* mat4_Scale_v( Matrix4* m, Vector3* v );

Matrix4* mat4_Translate( Matrix4* m, float x, float y, float z );
Matrix4* mat4_Translate_v( Matrix4* m, Vector3* v );

Matrix4* mat4_CreateOrthographicProjection( float left, float right, float top, float bottom, float nearPlane, float farPlane, Matrix4* out );
Matrix4* mat4_CreatePerspectiveProjection( float fovDeg, float aspectRatio, float nearPlane, float farPlane, Matrix4* out );
Matrix4* mat4_LookAtView( const Vector3* eyePos, const Vector3* lookPos, const Vector3* up, Matrix4* out );
Matrix4* mat4_FPSView( const Vector3* eyePos, float yaw, float pitch, Matrix4* out );

Vector3* mat4_TransformVec3Dir( const Matrix4* m, const Vector3* v, Vector3* out );
Vector3* mat4_TransformVec3Pos( const Matrix4* m, const Vector3* v, Vector3* out );
Vector3* mat4_TransformVec3Pos_InPlace( const Matrix4* m, Vector3* v );
Vector2* mat4_TransformVec2Pos( const Matrix4* m, const Vector2* v, Vector2* out );

bool mat4_Invert( const Matrix4* m, Matrix4* out );

int mat4_Compare( Matrix4* m, Matrix4* n );

void mat4_Dump( Matrix4* m, const char* extra  );

#endif // inclusion guard