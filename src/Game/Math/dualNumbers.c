#include "dualNumbers.h"

#include <SDL_assert.h>
#include <math.h>
#include "mathUtil.h"
#include "System/platformLog.h"

DualNumber dual( float r, float d )
{
	DualNumber dn;
	dn.real = r;
	dn.dual = d;
	return dn;
}

DualNumber* dual_Add( const DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	SDL_assert( lhs != NULL );
	SDL_assert( rhs != NULL );
	SDL_assert( out != NULL );

	out->real = lhs->real + rhs->real;
	out->dual = lhs->dual + rhs->dual;

	return out;
}

DualNumber* dual_Subtract( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	SDL_assert( lhs != NULL );
	SDL_assert( rhs != NULL );
	SDL_assert( out != NULL );

	out->real = lhs->real - rhs->real;
	out->dual = lhs->dual - rhs->dual;

	return out;
}

DualNumber* dual_Multiply( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out )
{
	SDL_assert( lhs != NULL );
	SDL_assert( rhs != NULL );
	SDL_assert( out != NULL );

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
	SDL_assert( lhs != NULL );
	SDL_assert( rhs != NULL );
	SDL_assert( out != NULL );

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
	SDL_assert( in != NULL );
	SDL_assert( out != NULL );

	out->real = -in->real;
	out->dual = -in->dual;

	return out;
}

// any differentiable function can be extended to dual numbers as f(a + be) = f(a) + b*f'(a)e
DualNumber* dual_Sin( const DualNumber* in, DualNumber* out )
{
	SDL_assert( in != NULL );
	SDL_assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	out->real = sinf( r );
	out->dual = d * cosf( r );

	return out;
}

DualNumber* dual_Cos( const DualNumber* in, DualNumber* out )
{
	SDL_assert( in != NULL );
	SDL_assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	out->real = cosf( r );
	out->dual = d * -sinf( r );

	return out;
}

DualNumber* dual_Tan( const DualNumber* in, DualNumber* out )
{
	SDL_assert( in != NULL );
	SDL_assert( out != NULL );

	float r = in->real;
	float d = in->dual;

	float t = tanf( r );

	out->real = t;
	out->dual = d * ( 1 + ( t * t ) );

	return out;
}

int dual_Compare( const DualNumber* lhs, const DualNumber* rhs )
{
	SDL_assert( lhs != NULL );
	SDL_assert( rhs != NULL );

	return ( FLT_LT( lhs->real, rhs->real ) ? -1 : ( FLT_GT( lhs->real, rhs->real ) ? 1 : 0 ) );
}

bool dual_Serialize( cmp_ctx_t* cmp, const DualNumber* dual )
{
	SDL_assert( dual != NULL );
	SDL_assert( cmp != NULL );

	if( !cmp_write_float( cmp, dual->real ) ) {
		llog( LOG_ERROR, "Unable to write real part of DualNumber." );
		return false;
	}

	if( !cmp_write_float( cmp, dual->dual ) ) {
		llog( LOG_ERROR, "Unable to write dual part of DualNumber." );
		return false;
	}

	return true;
}

bool dual_Deserialize( cmp_ctx_t* cmp, DualNumber* outDual )
{
	SDL_assert( outDual != NULL );
	SDL_assert( cmp != NULL );

	outDual->real = 0.0f;
	outDual->dual = 0.0f;

	if( !cmp_read_float( cmp, &( outDual->real ) ) ) {
		llog( LOG_ERROR, "Unable to load real part of DualNumber." );
		return false;
	}

	if( !cmp_read_float( cmp, &( outDual->dual ) ) ) {
		llog( LOG_ERROR, "Unable to load dual part of DualNumber." );
		return false;
	}

	return true;
}