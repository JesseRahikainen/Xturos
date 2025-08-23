#include "matrix3.h"
#include <SDL3/SDL_assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mathUtil.h"
#include "System/platformLog.h"

Matrix3* mat3_Multiply( const Matrix3* m, const Matrix3* n, Matrix3* out )
{
	SDL_assert( m != NULL );
	SDL_assert( n != NULL );
	SDL_assert( out != NULL );
	
	// temporary storage in case out is m or n
	Matrix3 temp;

	temp.m[0]  = ( m->m[0] * n->m[0] ) + ( m->m[3] * n->m[1] ) + ( m->m[6] * n->m[2] );
	temp.m[1]  = ( m->m[1] * n->m[0] ) + ( m->m[4] * n->m[1] ) + ( m->m[7] * n->m[2] );
	temp.m[2]  = ( m->m[2] * n->m[0] ) + ( m->m[5] * n->m[1] ) + ( m->m[8] * n->m[2] );

	temp.m[3]  = ( m->m[0] * n->m[3] ) + ( m->m[3] * n->m[4] ) + ( m->m[6] * n->m[5] );
	temp.m[4]  = ( m->m[1] * n->m[3] ) + ( m->m[4] * n->m[4] ) + ( m->m[7] * n->m[5] );
	temp.m[5]  = ( m->m[2] * n->m[3] ) + ( m->m[5] * n->m[4] ) + ( m->m[8] * n->m[5] );

	temp.m[6]  = ( m->m[0] * n->m[6] ) + ( m->m[3] * n->m[7] ) + ( m->m[6] * n->m[8] );
	temp.m[7]  = ( m->m[1] * n->m[6] ) + ( m->m[4] * n->m[7] ) + ( m->m[7] * n->m[8] );
	temp.m[8]  = ( m->m[2] * n->m[6] ) + ( m->m[5] * n->m[7] ) + ( m->m[8] * n->m[8] );

	memcpy( out, &temp, sizeof( Matrix3 ) );

	return out;
}

Vector3* mat3_TransformVec3Dir( const Matrix3* m, const Vector3* v, Vector3* out )
{
	SDL_assert( m != NULL );
	SDL_assert( v != NULL );
	SDL_assert( out != NULL );
	SDL_assert( v != out );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] ) + ( m->m[6] * v->v[2] );
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] ) + ( m->m[7] * v->v[2] );
	out->v[2] = ( m->m[2] * v->v[0] ) + ( m->m[5] * v->v[1] ) + ( m->m[8] * v->v[2] );

	return out;
}

void mat3_SetColumn( Matrix3* m, int column, const Vector3* col )
{
	SDL_assert( m != NULL );
	SDL_assert( col != NULL );
	SDL_assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	m->m[column+0] = col->v[0];
	m->m[column+1] = col->v[1];
	m->m[column+2] = col->v[2];
}

// sets the top two entries of the column to the passed in vector
void mat3_SetColumn2( Matrix3* m, int column, const Vector2* col )
{
	SDL_assert( m != NULL );
	SDL_assert( col != NULL );
	SDL_assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	m->m[column + 0] = col->v[0];
	m->m[column + 1] = col->v[1];
}

Vector3* mat3_GetColumn( const Matrix3* m, int column, Vector3* out )
{
	SDL_assert( m != NULL );
	SDL_assert( out != NULL );
	SDL_assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	out->v[0] = m->m[column+0];
	out->v[1] = m->m[column+1];
	out->v[2] = m->m[column+2];

	return out;
}

Vector2* mat3_GetColumn_2( const Matrix3* m, int column, Vector2* out )
{
	SDL_assert( m != NULL );
	SDL_assert( out != NULL );
	SDL_assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	out->v[0] = m->m[column+0];
	out->v[1] = m->m[column+1];

	return out;
}

Vector2* mat3_GetPosition( const Matrix3* m, Vector2* out )
{
	SDL_assert( m != NULL );
	SDL_assert( out != NULL );

	out->x = m->m[6] / m->m[8];
	out->y = m->m[7] / m->m[8];

	return out;
}

void mat3_SetPosition( Matrix3* m, const Vector2* pos )
{
	SDL_assert( m != NULL );
	SDL_assert( pos != NULL );

	m->m[6] = pos->x;
	m->m[7] = pos->y;
}

Matrix3* mat3_CreateTranslation( float fwd, float side, Matrix3* out )
{
	SDL_assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX_3, sizeof( Matrix3 ) );
	out->m[6] = fwd;
	out->m[7] = side;

	return out;
}

Matrix3* mat3_CreateScale( float scale, Matrix3* out )
{
	SDL_assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX_3, sizeof( Matrix3 ) );
	out->m[0] = scale;
	out->m[4] = scale;

	return out;
}

Matrix3* mat3_CreateScaleV( const Vector2* scale, Matrix3* out )
{
	SDL_assert( scale != NULL );
	SDL_assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX_3, sizeof( Matrix3 ) );
	out->m[0] = scale->x;
	out->m[4] = scale->y;

	return out;
}

Vector2* mat3_TransformVec2Dir( const Matrix3* m, const Vector2* v, Vector2* out )
{
	SDL_assert( m != NULL );
	SDL_assert( v != NULL );
	SDL_assert( out != NULL );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] );
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] );

	return out;
}

Vector2* mat3_TransformVec2Pos( const Matrix3* m, const Vector2* v, Vector2* out )
{
	SDL_assert( m != NULL );
	SDL_assert( v != NULL );
	SDL_assert( out != NULL );
	SDL_assert( out != v );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] ) + m->m[6];
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] ) + m->m[7];

	return out;
}

Vector2* mat3_TransformVec2Pos_InPlace( const Matrix3* m, Vector2* v )
{
	SDL_assert( m != NULL );
	SDL_assert( v != NULL );

	Vector2 temp;
	memcpy( &temp, v, sizeof( Vector2 ) );

	v->v[0] = ( m->m[0] * temp.v[0] ) + ( m->m[3] * temp.v[1] ) + m->m[2];
	v->v[1] = ( m->m[1] * temp.v[0] ) + ( m->m[4] * temp.v[1] ) + m->m[5];

	return v;
}

Matrix3* mat3_SetRotation( float rotDeg, Matrix3* out )
{
	SDL_assert( out != NULL );

	float sinRot = SDL_sinf( DEG_TO_RAD( rotDeg ) );
	float cosRot = SDL_cosf( DEG_TO_RAD( rotDeg ) );

	out->m[0] = cosRot;
	out->m[1] = sinRot;
	out->m[2] = 0.0f;
	
	out->m[3] = -sinRot;
	out->m[4] = cosRot;
	out->m[5] = 0.0f;
	
	out->m[6] = 0.0f;
	out->m[7] = 0.0f;
	out->m[8] = 1.0f;

	return out;
}

float mat3_Determinant( const Matrix3* m )
{
	SDL_assert( m != NULL );
	float det = ( m->m[0] * ( ( m->m[4] * m->m[8] ) - ( m->m[7] * m->m[5] ) ) ) -
		( m->m[3] * ( ( m->m[1] * m->m[8] ) - ( m->m[7] * m->m[2] ) ) ) -
		( m->m[6] * ( ( m->m[1] * m->m[5] ) - ( m->m[4] * m->m[2] ) ) );
	return det;
}

void mat3_TransformDecompose( const Matrix3* m, Vector2* outPos, float* outRotRad, Vector2* outScale )
{
	SDL_assert( m != NULL );
	SDL_assert( outPos != NULL );
	SDL_assert( outRotRad != NULL );
	SDL_assert( outScale != NULL );
	SDL_assert( m->m[2] == 0.0f && m->m[5] == 0.0f && m->m[8] == 1.0f ); // is affine

	// this doesn't handle skew
	// given a 3x3 2d affine transform matrix
	// [uc -vs x]
	// [us  vc y]
	// [ 0  0  1]
	// were c = cos(r), s = sin(r), u = x scale, v = y scale, x = x pos, y = y pos

	// pos is easy, just grab that column
	outPos->x = m->m[6];
	outPos->y = m->m[7];

	// scale is the magnitude of each of the axises of the upper-left 2x2
	outScale->x = SDL_sqrtf( ( m->m[0] * m->m[0] ) + ( m->m[1] * m->m[1] ) );
	outScale->y = SDL_sqrtf( ( m->m[3] * m->m[3] ) + ( m->m[4] * m->m[4] ) );

	// rotation
	( *outRotRad ) = SDL_atan2f( m->m[1], m->m[0] );
}

bool mat3_Inverse( const Matrix3* m, Matrix3* out )
{
	SDL_assert( m != NULL );
	SDL_assert( out != NULL );

	float det = mat3_Determinant( m );

	if( FLT_EQ( det, FLOAT_TOLERANCE ) ) {
		return false;
	}

	float invDet = 1 / det;
	out->m[0] = invDet * ( ( m->m[4] * m->m[8] ) - ( m->m[7] * m->m[5] ) );
	out->m[1] = invDet * ( ( m->m[7] * m->m[2] ) - ( m->m[1] * m->m[8] ) );
	out->m[2] = invDet * ( ( m->m[1] * m->m[5] ) - ( m->m[4] * m->m[2] ) );
	out->m[3] = invDet * ( ( m->m[6] * m->m[5] ) - ( m->m[3] * m->m[8] ) );
	out->m[4] = invDet * ( ( m->m[0] * m->m[8] ) - ( m->m[6] * m->m[2] ) );
	out->m[5] = invDet * ( ( m->m[3] * m->m[2] ) - ( m->m[0] * m->m[5] ) );
	out->m[6] = invDet * ( ( m->m[3] * m->m[7] ) - ( m->m[6] * m->m[4] ) );
	out->m[7] = invDet * ( ( m->m[6] * m->m[1] ) - ( m->m[0] * m->m[7] ) );
	out->m[8] = invDet * ( ( m->m[0] * m->m[4] ) - ( m->m[3] * m->m[1] ) );

	return true;
}

bool mat3_Serialize( cmp_ctx_t* cmp, const Matrix3* mat )
{
	SDL_assert( mat != NULL );
	SDL_assert( cmp != NULL );

	for( int i = 0; i < 16; ++i ) {
		if( !cmp_write_float( cmp, mat->m[i] ) ) {
			llog( LOG_ERROR, "Unable to write entry %i in Matrix4.", i );
			return false;
		}
	}

	return true;
}

bool mat3_Deserialize( cmp_ctx_t* cmp, Matrix3* outMat )
{
	SDL_assert( outMat != NULL );
	SDL_assert( cmp != NULL );

	*outMat = ZERO_MATRIX_3;

	for( int i = 0; i < 16; ++i ) {
		if( !cmp_read_float( cmp, &( outMat->m[i] ) ) ) {
			llog( LOG_ERROR, "Unable to read entry %i in Matrix4.", i );
			return false;
		}
	}

	return true;
}

Matrix3* mat3_CreateTransform( const Vector2* pos, float rotRad, const Vector2* scale, Matrix3* out )
{
	SDL_assert( out != NULL );
	SDL_assert( pos != NULL );
	SDL_assert( scale != NULL );

	// rotation/scale
	float sinRot = SDL_sinf( rotRad );
	float cosRot = SDL_cosf( rotRad );

	out->m[0] = cosRot * scale->x;
	out->m[1] = sinRot * scale->x;
	out->m[2] = 0.0f;

	out->m[3] = -sinRot * scale->y;
	out->m[4] = cosRot * scale->y;
	out->m[5] = 0.0f;

	// position
	out->m[6] = pos->x;
	out->m[7] = pos->y;
	out->m[8] = 1.0f;

	return out;
}

Matrix3* mat3_CreateRenderTransform( const Vector2* pos, float rotRad, const Vector2* offset, const Vector2* scale, Matrix3* out )
{
	SDL_assert( out != NULL );
	SDL_assert( pos != NULL );
	SDL_assert( offset != NULL );
	SDL_assert( scale != NULL );

	// this is the fully multiplied out multiplication of the position, scale, and rotation
	// pos * rot * offset * scale
	float sinRot = SDL_sinf( rotRad );
	float cosRot = SDL_cosf( rotRad );

	out->m[0] = cosRot * scale->x;
	out->m[1] = sinRot * scale->x;
	out->m[2] = 0.0f;

	out->m[3] = -sinRot * scale->y;
	out->m[4] = cosRot * scale->y;
	out->m[5] = 0.0f;

	out->m[6] = pos->x -( offset->y * sinRot ) + ( offset->x * cosRot );
	out->m[7] = pos->y + ( offset->x * sinRot ) + ( offset->y * cosRot );
	out->m[8] = 1.0f;

	return out;
}

// For debugging.
void mat3_Dump( Matrix3* m, const char* extra )
{
	SDL_assert( m != NULL );

	if( extra != NULL ) {
		llog( LOG_DEBUG, "%s", extra );
	}
	llog( LOG_DEBUG, "m4 = %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f\n           %7.3f %7.3f %7.3f",
		m->m[0], m->m[3], m->m[6],
		m->m[1], m->m[4], m->m[7],
		m->m[2], m->m[5], m->m[8] );
}