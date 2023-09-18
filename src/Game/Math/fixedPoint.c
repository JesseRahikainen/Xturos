#include "fixedPoint.h"

#include <math.h>
#include <SDL_assert.h>

#include "System/platformLog.h"

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

fixed32 f32_FromParts( int16_t whole, uint16_t fraction )
{
	fixed32 val;
	val = ( (int32_t)whole << Q ) | ( (int32_t)fraction );
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

bool f32_Serialize( cmp_ctx_t* cmp, fixed32 fp )
{
	SDL_assert( cmp != NULL );

	if( !cmp_write_s32( cmp, fp ) ) {
		llog( LOG_ERROR, "Unable to write fixed point." );
		return false;
	}

	return true;
}

bool f32_Deserialize( cmp_ctx_t* cmp, fixed32* outFp )
{
	SDL_assert( cmp != NULL );

	*outFp = 0;

	if( !cmp_read_s32( cmp, outFp ) ) {
		llog( LOG_ERROR, "Unable to load fixed point." );
		return false;
	}

	return true;
}