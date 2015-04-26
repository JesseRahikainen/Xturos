#include "color.h"
#include <assert.h>
#include "../Math/mathUtil.h"

Color* clr_Lerp( const Color* from, const Color* to, float t, Color* out )
{
	assert( from != NULL );
	assert( to != NULL );
	assert( out != NULL );

	out->r = lerp( from->r, to->r, t );
	out->g = lerp( from->g, to->g, t );
	out->b = lerp( from->b, to->b, t );
	out->a = lerp( from->a, to->a, t );

	return out;
}

SDL_Color clr_ToSDLColor( const Color* color )
{
	assert( color );

	SDL_Color out;

	out.r = lerp_uint8_t( 0, 255, color->r );
	out.g = lerp_uint8_t( 0, 255, color->g );
	out.b = lerp_uint8_t( 0, 255, color->b );
	out.a = lerp_uint8_t( 0, 255, color->a );

	return out;
}