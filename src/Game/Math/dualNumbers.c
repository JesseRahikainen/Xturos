#include "dualNumbers.h"

#include <assert.h>
#include <math.h>
#include "mathUtil.h"

DualNumber dual( float r, float d )
{
	DualNumber dn;
	dn.real = r;
	dn.dual = d;
	return dn;
}

DualNumber* dual_Add( const DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	out->real = lhs->real + rhs->real;
	out->dual = lhs->dual + rhs->dual;

	return out;
}

DualNumber* dual_Subtract( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	out->real = lhs->real - rhs->real;
	out->dual = lhs->dual - rhs->dual;

	return out;
}

DualNumber* dual_Multiply( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	float lr = lhs->real;
	float rr = rhs->real;
	float ld = lhs->dual;
	float rd = rhs->dual;

	out->real = lr * rr;
	out->dual = ( ld * rr ) + ( lr * rd );

	return out;
}

DualNumber* dual_Divide( const DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	assert( lhs != NULL );
	assert( rhs != NULL );
	assert( out != NULL );

	float lr = lhs->real;
	float rr = rhs->real;
	float ld = lhs->dual;
	float rd = rhs->dual;

	out->real = lr / rr;
	out->dual = ( ( ld * rr ) - ( lr * rd ) ) / ( lr * lr );

	return out;
}

DualNumber* dual_Negate( const DualNumber* in, DualNumber* out )
{
	assert( in != NULL );
	assert( out != NULL );

	out->real = -in->real;
	out->dual = -in->dual;

	return out;
}

// any differentiable function can be extended to dual numbers as f(a + be) = f(a) + b*f'(a)e
DualNumber* dual_Sin( const DualNumber* in, DualNumber* out )
{
	assert( in != NULL );
	assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	out->real = sinf( r );
	out->dual = d * cosf( r );

	return out;
}

DualNumber* dual_Cos( const DualNumber* in, DualNumber* out )
{
	assert( in != NULL );
	assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	out->real = cosf( r );
	out->dual = d * -sinf( r );

	return out;
}

DualNumber* dual_Tan( const DualNumber* in, DualNumber* out )
{
	assert( in != NULL );
	assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	float t = tanf( r );

	out->real = t;
	out->dual = d * ( 1 + ( t * t ) );

	return out;
}

int dual_Compare( const DualNumber* lhs, const DualNumber* rhs )
{
	assert( lhs != NULL );
	assert( rhs != NULL );

	return ( FLT_LT( lhs->real, rhs->real ) ? -1 : ( FLT_GT( lhs->real, rhs->real ) ? 1 : 0 ) );
}