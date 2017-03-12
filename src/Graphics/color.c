#include "color.h"
#include <assert.h>
#include "../Math/mathUtil.h"

Color clr( float r, float g, float b, float a )
{
	Color c;

	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;

	return c;
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

Color* clr_Scale( const Color* color, float scale, Color* out )
{
	assert( color != NULL );
	assert( out != NULL );

	out->r = color->r * scale;
	out->g = color->g * scale;
	out->b = color->b * scale;
	out->a = color->a * scale;

	return out;
}

Color* clr_AddScaled( const Color* base, const Color* scaled, float scale, Color* out )
{
	assert( base != NULL );
	assert( scaled != NULL );
	assert( out != NULL );

	out->r = base->r + ( scaled->r * scale );
	out->g = base->g + ( scaled->g * scale );
	out->b = base->b + ( scaled->b * scale );
	out->a = base->a + ( scaled->a * scale );

	return out;
}