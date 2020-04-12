#include "matrix3.h"
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mathUtil.h"

Matrix3* mat3_Multiply( const Matrix3* m, const Matrix3* n, Matrix3* out )
{
	assert( m != NULL );
	assert( n != NULL );
	assert( out != NULL );
	
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
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );
	assert( v != out );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] ) + ( m->m[6] * v->v[2] );
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] ) + ( m->m[7] * v->v[2] );
	out->v[2] = ( m->m[2] * v->v[0] ) + ( m->m[5] * v->v[1] ) + ( m->m[8] * v->v[2] );

	return out;
}

void mat3_SetColumn( Matrix3* m, int column, const Vector3* col )
{
	assert( m != NULL );
	assert( col != NULL );
	assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	m->m[column+0] = col->v[0];
	m->m[column+1] = col->v[1];
	m->m[column+2] = col->v[2];
}

Vector3* mat3_GetColumn( const Matrix3* m, int column, Vector3* out )
{
	assert( m != NULL );
	assert( out != NULL );
	assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	out->v[0] = m->m[column+0];
	out->v[1] = m->m[column+1];
	out->v[2] = m->m[column+2];

	return out;
}

Vector2* mat3_GetColumn_2( const Matrix3* m, int column, Vector2* out )
{
	assert( m != NULL );
	assert( out != NULL );
	assert( ( column >= 0 ) && ( column < 3 ) );

	column *= 3;
	out->v[0] = m->m[column+0];
	out->v[1] = m->m[column+1];

	return out;
}

Vector2* mat3_GetPosition( const Matrix3* m, Vector2* out )
{
	assert( m != NULL );
	assert( out != NULL );

	out->x = m->m[6] / m->m[8];
	out->y = m->m[7] / m->m[8];

	return out;
}

void mat3_SetPosition( Matrix3* m, const Vector2* pos )
{
	assert( m != NULL );
	assert( pos != NULL );

	m->m[6] = pos->x;
	m->m[7] = pos->y;
}

Matrix3* mat3_CreateTranslation( float fwd, float side, Matrix3* out )
{
	assert( out != NULL );

	memcpy( out, &IDENTITY_MATRIX_3, sizeof( Matrix3 ) );
	out->m[6] = fwd;
	out->m[7] = side;

	return out;
}

Vector2* mat3_TransformVec2Dir( const Matrix3* m, const Vector2* v, Vector2* out )
{
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] );
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] );

	return out;
}

Vector2* mat3_TransformVec2Pos( const Matrix3* m, const Vector2* v, Vector2* out )
{
	assert( m != NULL );
	assert( v != NULL );
	assert( out != NULL );

	out->v[0] = ( m->m[0] * v->v[0] ) + ( m->m[3] * v->v[1] ) + m->m[6];
	out->v[1] = ( m->m[1] * v->v[0] ) + ( m->m[4] * v->v[1] ) + m->m[7];

	return out;
}

Vector2* mat3_TransformVec2Pos_InPlace( const Matrix3* m, Vector2* v )
{
	assert( m != NULL );
	assert( v != NULL );

	Vector2 temp;
	memcpy( &temp, v, sizeof( Vector2 ) );

	v->v[0] = ( m->m[0] * temp.v[0] ) + ( m->m[3] * temp.v[1] ) + m->m[2];
	v->v[1] = ( m->m[1] * temp.v[0] ) + ( m->m[4] * temp.v[1] ) + m->m[5];

	return v;
}

Matrix3* mat3_SetRotation( float rotDeg, Matrix3* out )
{
	assert( out != NULL );

	float sinRot = sinf( DEG_TO_RAD( rotDeg ) );
	float cosRot = cosf( DEG_TO_RAD( rotDeg ) );

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

bool mat3_Inverse( const Matrix3* m, Matrix3* out )
{
	assert( m != NULL );
	assert( out != NULL );

	float det = ( m->m[0] * ( ( m->m[4] * m->m[8] ) - ( m->m[7] * m->m[5] ) ) ) -
		( m->m[3] * ( ( m->m[1] * m->m[8] ) - ( m->m[7] * m->m[2] ) ) ) -
		( m->m[6] * ( ( m->m[1] * m->m[5] ) - ( m->m[4] * m->m[2] ) ) );

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