#include <string.h>
#include <math.h>
#include <assert.h>
#include "matrix4.h"
#include "../System/platformLog.h"

Matrix4* mat4_Multiply( const Matrix4* m, const Matrix4* n, Matrix4* out )
{
	assert( m != NULL );
	assert( n != NULL );
	assert( out != NULL );

	// used a temp to make sure if m or n are out we don't override in the middle of doing stuff
	Matrix4 temp;

	temp.m[0]  = ( m->m[0] * n->m[0]  ) + ( m->m[4] *  n->m[1] ) + (  m->m[8] *  n->m[2] ) + ( m->m[12] *  n->m[3] );
	temp.m[1]  = ( m->m[1] * n->m[0]  ) + ( m->m[5] *  n->m[1] ) + (  m->m[9] *  n->m[2] ) + ( m->m[13] *  n->m[3] );
	temp.m[2]  = ( m->m[2] * n->m[0]  ) + ( m->m[6] *  n->m[1] ) + ( m->m[10] *  n->m[2] ) + ( m->m[14] *  n->m[3] );
	temp.m[3]  = ( m->m[3] * n->m[0]  ) + ( m->m[7] *  n->m[1] ) + ( m->m[11] *  n->m[2] ) + ( m->m[15] *  n->m[3] );

	temp.m[4]  = ( m->m[0] * n->m[4]  ) + ( m->m[4] *  n->m[5] ) + (  m->m[8] *  n->m[6] ) + ( m->m[12] *  n->m[7] );
	temp.m[5]  = ( m->m[1] * n->m[4]  ) + ( m->m[5] *  n->m[5] ) + (  m->m[9] *  n->m[6] ) + ( m->m[13] *  n->m[7] );
	temp.m[6]  = ( m->m[2] * n->m[4]  ) + ( m->m[6] *  n->m[5] ) + ( m->m[10] *  n->m[6] ) + ( m->m[14] *  n->m[7] );
	temp.m[7]  = ( m->m[3] * n->m[4]  ) + ( m->m[7] *  n->m[5] ) + ( m->m[11] *  n->m[6] ) + ( m->m[15] *  n->m[7] );

	temp.m[8]  = ( m->m[0] * n->m[8]  ) + ( m->m[4] *  n->m[9] ) + (  m->m[8] * n->m[10] ) + ( m->m[12] * n->m[11] );
	temp.m[9]  = ( m->m[1] * n->m[8]  ) + ( m->m[5] *  n->m[9] ) + (  m->m[9] * n->m[10] ) + ( m->m[13] * n->m[11] );
	temp.m[10] = ( m->m[2] * n->m[8]  ) + ( m->m[6] *  n->m[9] ) + ( m->m[10] * n->m[10] ) + ( m->m[14] * n->m[11] );
	temp.m[11] = ( m->m[3] * n->m[8]  ) + ( m->m[7] *  n->m[9] ) + ( m->m[11] * n->m[10] ) + ( m->m[15] * n->m[11] );

	temp.m[12] = ( m->m[0] * n->m[12] ) + ( m->m[4] * n->m[13] ) + (  m->m[8] * n->m[14] ) + ( m->m[12] * n->m[15] );
	temp.m[13] = ( m->m[1] * n->m[12] ) + ( m->m[5] * n->m[13] ) + (  m->m[9] * n->m[14] ) + ( m->m[13] * n->m[15] );
	temp.m[14] = ( m->m[2] * n->m[12] ) + ( m->m[6] * n->m[13] ) + ( m->m[10] * n->m[14] ) + ( m->m[14] * n->m[15] );
	temp.m[15] = ( m->m[3] * n->m[12] ) + ( m->m[7] * n->m[13] ) + ( m->m[11] * n->m[14] ) + ( m->m[15] * n->m[15] );

	memcpy( out, &temp, sizeof( Matrix4 ) );

	return out;
}

Matrix4* mat4_CreateXRotation( float angleRad, Matrix4* out )
{
	assert( out != NULL );

	float sine = (float)sin( angleRad );
	float cosine = (float)cos( angleRad );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	out->m[5] = cosine;
	out->m[6] = sine;
	out->m[9] = -sine;
	out->m[10] = cosine;

	return out;
}

Matrix4* mat4_CreateYRotation( float angleRad, Matrix4* out )
{
	assert( out != NULL );

	float sine = (float)sin( angleRad );
	float cosine = (float)cos( angleRad );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	out->m[0] = cosine;
	out->m[2] = -sine;
	out->m[8] = sine;
	out->m[10] = cosine;

	return out;
}

Matrix4* mat4_CreateZRotation( float angleRad, Matrix4* out )
{
	assert( out != NULL );

	float sine = (float)sin( angleRad );
	float cosine = (float)cos( angleRad );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	out->m[0] = cosine;
	out->m[1] = sine;
	out->m[4] = -sine;
	out->m[5] = cosine;

	return out;
}

Matrix4* mat4_CreateTranslation( float x, float y, float z, Matrix4* out )
{
	assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	out->m[12] = x;
	out->m[13] = y;
	out->m[14] = z;

	return out;
}

Matrix4* mat4_CreateTranslation_v( const Vector3* v, Matrix4* out )
{
	assert( v != NULL );
	assert( out != NULL );

	return mat4_CreateTranslation( v->v[0], v->v[1], v->v[2], out );
}

Matrix4* mat4_CreateScale( float x, float y, float z, Matrix4* out )
{
	assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	out->m[0] = x;
	out->m[5] = y;
	out->m[10] = z;

	return out;
}

Matrix4* mat4_CreateScale_v( const Vector3* v, Matrix4* out )
{
	assert( v != NULL );
	assert( out != NULL );

	return mat4_CreateScale( v->v[0], v->v[1], v->v[2], out );
}

Matrix4* mat4_Scale( Matrix4* m, float x, float y, float z )
{
	assert( m != NULL );

	Matrix4 scale, original;

	memcpy( &scale, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	scale.m[0] = x;
	scale.m[5] = y;
	scale.m[10] = z;

	// need to create a copy of the original matrix to avoid problems with overwriting during the multiplication
	memcpy( &original, m, sizeof( Matrix4 ) );

	return mat4_Multiply( &original, &scale, m );
}

Matrix4* mat4_Scale_v( Matrix4* m, Vector3* v )
{
	assert( m != NULL );
	assert( v != NULL );

	return mat4_Scale( m, v->v[0], v->v[1], v->v[2] );
}

Matrix4* mat4_Translate( Matrix4* m, float x, float y, float z )
{
	assert( m != NULL );

	Matrix4 translation, original;
	
	memcpy( &translation, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	translation.m[12] = x;
	translation.m[13] = y;
	translation.m[14] = z;

	// need to create a copy of the original matrix to avoid problems with overwriting during the multiplication
	memcpy( &original, m, sizeof( Matrix4 ) );

	return mat4_Multiply( &original, &translation, m );
}

Matrix4* mat4_Translate_v( Matrix4* m, Vector3* v )
{
	assert( m != NULL );
	assert( v != NULL );

	return mat4_Translate( m, v->v[0], v->v[1], v->v[2] );
}

// The projection formulas are taken from 3-D Computer Graphics: A Mathematical Introduction with OpenGL pg 55
Matrix4* mat4_CreateOrthographicProjection( float left, float right, float top, float bottom, float near, float far, Matrix4* out )
{
	assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );

	float rml = right - left;
	float tmb = top - bottom;
	float fmn = far - near;

	out->m[0] = 2.0f / rml;
	out->m[5] = 2.0f / tmb;
	out->m[10] = -2.0f / fmn;

	out->m[12] = -( right + left ) / rml;
	out->m[13] = -( top + bottom ) / tmb;
	out->m[14] = -( far + near ) / fmn;

	return out;
}

//  based on the OpenGL.org gluPerspective man page
Matrix4* mat4_CreatePerspectiveProjection( float fovDeg, float aspectRatio, float near, float far, Matrix4* out )
{
	assert( out != NULL );

	memset( out, 0, sizeof( Matrix4 ) );

	float f = cosf( DEG_TO_RAD( fovDeg ) / 2.0f ) / sinf( DEG_TO_RAD( fovDeg ) / 2.0f );

	out->m[0] = f / aspectRatio;
	out->m[5] = f;
	out->m[10] = -( far + near ) / ( far - near );
	out->m[11] = -1.0f;
	out->m[14] = ( -2.0f * far * near ) / ( far - near );

	return out;
}

Matrix4* mat4_LookAtView( const Vector3* eyePos, const Vector3* lookPos, const Vector3* up, Matrix4* out )
{
	assert( out != NULL );

	Vector3 u, f, s;

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );

	vec3_Subtract( lookPos, eyePos, &f );
	vec3_Normalize( &f );

	memcpy( &u, up, sizeof( Vector3 ) );
	vec3_Normalize( &u );

	vec3_CrossProd( &f, &u, &s );
	vec3_Normalize( &s );

	vec3_CrossProd( &s, &f, &u );

	out->m[0] = s.v[0];
	out->m[4] = s.v[1];
	out->m[8] = s.v[2];

	out->m[1] = u.v[0];
	out->m[5] = u.v[1];
	out->m[9] = u.v[2];

	out->m[2] = -f.v[0];
	out->m[6] = -f.v[1];
	out->m[10] = -f.v[2];

	out->m[12] = -vec3_DotProd( &s, eyePos );
	out->m[13] = -vec3_DotProd( &u, eyePos );
	out->m[14] = vec3_DotProd( &f, eyePos );

	return out;
}

Matrix4* mat4_FPSView( const Vector3* eyePos, float yaw, float pitch, Matrix4* out )
{
	assert( out != NULL );

	while( yaw > 360.0f ) yaw -= 360.0f;
	while( yaw <= 0.0f ) yaw += 360.0f;
	pitch = ( pitch < -90.0f ) ? -90.0f : pitch;
	pitch = ( pitch > 90.0f ) ? 90.0f : pitch;

    float cosPitch = cosf( DEG_TO_RAD( pitch ) );
    float sinPitch = sinf( DEG_TO_RAD( pitch ) );
    float cosYaw = cosf( DEG_TO_RAD( yaw ) );
    float sinYaw = sinf( DEG_TO_RAD( yaw ) );

    Vector3 s;
    Vector3 u;
    Vector3 f;

	s.v[0] = cosYaw;
	s.v[1] = 0.0f;
	s.v[2] = -sinYaw;

	u.v[0] = sinYaw * sinPitch;
	u.v[1] = cosPitch;
	u.v[2] = cosYaw * sinPitch;

	f.v[0] = sinYaw * cosPitch;
	f.v[1] = -sinPitch;
	f.v[2] = cosPitch * cosYaw;

	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );

	out->m[0]  = s.v[0];
	out->m[1]  = u.v[0];
	out->m[2]  = f.v[0];

	out->m[4]  = s.v[1];
	out->m[5]  = u.v[1];
	out->m[6]  = f.v[1];

	out->m[8]  = s.v[2];
	out->m[9]  = u.v[2];
	out->m[10] = f.v[2];

	out->m[12] = -vec3_DotProd( &s, eyePos );
	out->m[13] = -vec3_DotProd( &u, eyePos );
	out->m[14] = -vec3_DotProd( &f, eyePos );

	return out;
}

Vector3* mat4_TransformVec3Dir( const Matrix4* m, const Vector3* v, Vector3* out )
{
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );
	assert( v != out );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[4] * v->v[1] ) + (  m->m[8] * v->v[2] );
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[5] * v->v[1] ) + (  m->m[9] * v->v[2] );
	out->v[2] = ( m->m[2] * v->v[0] ) + ( m->m[6] * v->v[1] ) + ( m->m[10] * v->v[2] );

	return out;
}

Vector3* mat4_TransformVec3Pos( const Matrix4* m, const Vector3* v, Vector3* out )
{
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );
	assert( v != out );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[4] * v->v[1] ) + (  m->m[8] * v->v[2] ) + m->m[12];
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[5] * v->v[1] ) + (  m->m[9] * v->v[2] ) + m->m[13];
	out->v[2] = ( m->m[2] * v->v[0] ) + ( m->m[6] * v->v[1] ) + ( m->m[10] * v->v[2] ) + m->m[14];

	return out;
}

Vector3* mat4_TransformVec3Pos_InPlace( const Matrix4* m, Vector3* v )
{
	assert( m != NULL );
	assert( v != NULL );

	Vector3 temp;
	memcpy( &temp, v, sizeof( Vector3 ) );

	v->v[0] = ( m->m[0] * temp.v[0] ) + ( m->m[4] * temp.v[1] ) + (  m->m[8] * temp.v[2] ) + m->m[12];
	v->v[1] = ( m->m[1] * temp.v[0] ) + ( m->m[5] * temp.v[1] ) + (  m->m[9] * temp.v[2] ) + m->m[13];
	v->v[2] = ( m->m[2] * temp.v[0] ) + ( m->m[6] * temp.v[1] ) + ( m->m[10] * temp.v[2] ) + m->m[14];

	return v;
}

Vector2* mat4_TransformVec2Pos( const Matrix4* m, const Vector2* v, Vector2* out )
{
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );
	assert( v != out );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[4] * v->v[1] ) + m->m[12];
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[5] * v->v[1] ) + m->m[13];

	return out;
}

bool mat4_Invert( const Matrix4* m, Matrix4* out )
{
	assert( m != NULL );
	assert( out != NULL );

	Matrix4 inv;
	float det;

	inv.m[0] =
		m->m[5]  * m->m[10] * m->m[15] -
		m->m[5]  * m->m[11] * m->m[14] -
		m->m[9]  * m->m[6]  * m->m[15] +
		m->m[9]  * m->m[7]  * m->m[14] +
		m->m[13] * m->m[6]  * m->m[11] -
		m->m[13] * m->m[7]  * m->m[10];

	inv.m[4] =
		-m->m[4]  * m->m[10] * m->m[15] +
		 m->m[4]  * m->m[11] * m->m[14] +
		 m->m[8]  * m->m[6]  * m->m[15] -
		 m->m[8]  * m->m[7]  * m->m[14] -
		 m->m[12] * m->m[6]  * m->m[11] +
		 m->m[12] * m->m[7]  * m->m[10];

	inv.m[8] =
		m->m[4]  * m->m[9]  * m->m[15] -
		m->m[4]  * m->m[11] * m->m[13] -
		m->m[8]  * m->m[5]  * m->m[15] +
		m->m[8]  * m->m[7]  * m->m[13] +
		m->m[12] * m->m[5]  * m->m[11] -
		m->m[12] * m->m[7]  * m->m[9];

	inv.m[12] =
		-m->m[4]  * m->m[9]  * m->m[14] +
		 m->m[4]  * m->m[10] * m->m[13] +
		 m->m[8]  * m->m[5]  * m->m[14] -
		 m->m[8]  * m->m[6]  * m->m[13] -
		 m->m[12] * m->m[5]  * m->m[10] +
		 m->m[12] * m->m[6]  * m->m[9];

	inv.m[1] =
		-m->m[1]  * m->m[10] * m->m[15] +
		 m->m[1]  * m->m[11] * m->m[14] +
		 m->m[9]  * m->m[2]  * m->m[15] -
		 m->m[9]  * m->m[3]  * m->m[14] -
		 m->m[13] * m->m[2]  * m->m[11] +
		 m->m[13] * m->m[3]  * m->m[10];

	inv.m[5] =
		m->m[0]  * m->m[10] * m->m[15] -
		m->m[0]  * m->m[11] * m->m[14] -
		m->m[8]  * m->m[2]  * m->m[15] +
		m->m[8]  * m->m[3]  * m->m[14] +
		m->m[12] * m->m[2]  * m->m[11] -
		m->m[12] * m->m[3]  * m->m[10];

	inv.m[9] =
		-m->m[0]  * m->m[9]  * m->m[15] +
		 m->m[0]  * m->m[11] * m->m[13] +
		 m->m[8]  * m->m[1]  * m->m[15] -
		 m->m[8]  * m->m[3]  * m->m[13] -
		 m->m[12] * m->m[1]  * m->m[11] +
		 m->m[12] * m->m[3]  * m->m[9];

	inv.m[13] =
		m->m[0]  * m->m[9]  * m->m[14] -
		m->m[0]  * m->m[10] * m->m[13] -
		m->m[8]  * m->m[1]  * m->m[14] +
		m->m[8]  * m->m[2]  * m->m[13] +
		m->m[12] * m->m[1]  * m->m[10] -
		m->m[12] * m->m[2]  * m->m[9];

	inv.m[2] =
		m->m[1]  * m->m[6] * m->m[15] -
		m->m[1]  * m->m[7] * m->m[14] -
		m->m[5]  * m->m[2] * m->m[15] +
		m->m[5]  * m->m[3] * m->m[14] +
		m->m[13] * m->m[2] * m->m[7] -
		m->m[13] * m->m[3] * m->m[6];

	inv.m[6] =
		-m->m[0]  * m->m[6] * m->m[15] +
	 	 m->m[0]  * m->m[7] * m->m[14] +
		 m->m[4]  * m->m[2] * m->m[15] -
		 m->m[4]  * m->m[3] * m->m[14] -
		 m->m[12] * m->m[2] * m->m[7] +
		 m->m[12] * m->m[3] * m->m[6];

	inv.m[10] =
		m->m[0]  * m->m[5] * m->m[15] -
		m->m[0]  * m->m[7] * m->m[13] -
		m->m[4]  * m->m[1] * m->m[15] +
		m->m[4]  * m->m[3] * m->m[13] +
		m->m[12] * m->m[1] * m->m[7] -
		m->m[12] * m->m[3] * m->m[5];

	inv.m[14] =
		-m->m[0]  * m->m[5] * m->m[14] +
		 m->m[0]  * m->m[6] * m->m[13] +
		 m->m[4]  * m->m[1] * m->m[14] -
		 m->m[4]  * m->m[2] * m->m[13] -
		 m->m[12] * m->m[1] * m->m[6] +
		 m->m[12] * m->m[2] * m->m[5];

	inv.m[3] =
		-m->m[1] * m->m[6] * m->m[11] +
		 m->m[1] * m->m[7] * m->m[10] +
		 m->m[5] * m->m[2] * m->m[11] -
		 m->m[5] * m->m[3] * m->m[10] -
		 m->m[9] * m->m[2] * m->m[7] +
		 m->m[9] * m->m[3] * m->m[6];

	inv.m[7] =
		m->m[0] * m->m[6] * m->m[11] -
		m->m[0] * m->m[7] * m->m[10] -
		m->m[4] * m->m[2] * m->m[11] +
		m->m[4] * m->m[3] * m->m[10] +
		m->m[8] * m->m[2] * m->m[7] -
		m->m[8] * m->m[3] * m->m[6];

	inv.m[11] =
		-m->m[0] * m->m[5] * m->m[11] +
		 m->m[0] * m->m[7] * m->m[9] +
		 m->m[4] * m->m[1] * m->m[11] -
		 m->m[4] * m->m[3] * m->m[9] -
		 m->m[8] * m->m[1] * m->m[7] +
		 m->m[8] * m->m[3] * m->m[5];

	inv.m[15] =
		m->m[0] * m->m[5] * m->m[10] -
		m->m[0] * m->m[6] * m->m[9] -
		m->m[4] * m->m[1] * m->m[10] +
		m->m[4] * m->m[2] * m->m[9] +
		m->m[8] * m->m[1] * m->m[6] -
		m->m[8] * m->m[2] * m->m[5];

	det = m->m[0] * inv.m[0] + m->m[1] * inv.m[4] + m->m[2] * inv.m[8] + m->m[3] * inv.m[12];

	if( FLT_EQ( det, FLOAT_TOLERANCE ) ) return false;

	det = 1.0f / det;

	for( int i = 0; i < 16; i++ ) {
		out->m[i] = inv.m[i] * det;
	}

	return true;
}


int mat4_Compare( Matrix4* m, Matrix4* n )
{
	assert( m != NULL );
	assert( n != NULL );

	for( int i = 0; i < 16; ++i ) {
		if( fabsf( m->m[i] - n->m[i] ) > 0.0001f ) {
			return 0;
		}
	}

	return 1;
}

// For debugging.
void mat4_Dump( Matrix4* m, const char* extra )
{
	assert( m != NULL );

	if( extra != NULL ) {
		llog( LOG_DEBUG,  "%s", extra );
	}
	llog( LOG_DEBUG,  "m4 = %7.3f %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f %7.3f",
		m->m[0], m->m[4], m->m[8],  m->m[12],
		m->m[1], m->m[5], m->m[9],  m->m[13],
		m->m[2], m->m[6], m->m[10], m->m[14],
		m->m[3], m->m[7], m->m[11], m->m[15] );
}