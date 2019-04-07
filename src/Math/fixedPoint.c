#include "fixedPoint.h"

#include <math.h>

// will be in the Q16.16 format
//  https://en.wikipedia.org/wiki/Q_(number_format)

#define Q 16
#define BASE ( 1 << Q )
#define INV_BASE ( 1.0f / (float)BASE )
#define K ( 1 << ( Q - 1 ) )

static fixed32 saturate( int64_t val )
{
	if( val > INT32_MAX ) return INT32_MAX;
	if( val < INT32_MIN ) return INT32_MIN;
	return (fixed32)val;
}

fixed32 f32_FromFloat( float f )
{
	fixed32 val;
	f *= BASE;
	val = (fixed32)roundf( f );
	return val;
}

float f32_FromFixedPoint( fixed32 fp )
{
	float val = (float)fp;
	val *= INV_BASE;
	return val;
}

fixed32 f32_Add( fixed32 lhs, fixed32 rhs )
{
	return lhs + rhs;
}

fixed32 f32_SaturatedAdd( fixed32 lhs, fixed32 rhs )
{
	return saturate( (int64_t)lhs + (int64_t)rhs );
}

fixed32 f32_Subtract( fixed32 lhs, fixed32 rhs )
{
	return lhs - rhs;
}

fixed32 f32_Multiply( fixed32 lhs, fixed32 rhs )
{
	int64_t temp;

	temp = (int64_t)lhs * (int64_t)rhs;
	temp += K;
	return saturate( temp >> Q );
}

fixed32 f32_Divide( fixed32 lhs, fixed32 rhs )
{
	int64_t temp;

	temp = (int64_t)lhs << Q;
	if( ( ( temp >= 0 ) && ( rhs >= 0 ) ) || ( ( temp < 0 ) && ( rhs < 0 ) ) ) {
		temp += rhs / 2;
	} else {
		temp -= rhs / 2;
	}

	return (fixed32)( temp / rhs );
}