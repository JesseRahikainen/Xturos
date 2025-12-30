#include "dualNumbers.h"

#include <SDL3/SDL_assert.h>
#include <math.h>
#include "mathUtil.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"

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

bool dual_Serialize( Serializer* s, const char* name, DualNumber* dual )
{
	ASSERT_AND_IF_NOT( s != NULL ) return false;
	ASSERT_AND_IF_NOT( dual != NULL ) return false;

	if( !s->startStructure( s, name ) ) {
		llog( LOG_ERROR, "Unable to start DualNumber." );
		return false;
	}

	if( !s->flt( s, "real", &(dual->real) ) ) {
		llog( LOG_ERROR, "Unable to serialize real part of DualNumber." );
		return false;
	}

	if( !s->flt( s, "dual", &( dual->dual ) ) ) {
		llog( LOG_ERROR, "Unable to serialize dual part of DualNumber." );
		return false;
	}

	if( !s->endStructure( s, name ) ) {
		llog( LOG_ERROR, "Unable to end DualNumber." );
		return false;
	}

	return true;
}