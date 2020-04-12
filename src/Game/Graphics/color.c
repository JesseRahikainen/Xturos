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

Color clr_byte( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
	Color c;

	c.r = r / 255.0f;
	c.g = g / 255.0f;
	c.b = b / 255.0f;
	c.a = a / 255.0f;

	return c;
}

Color clr_hex( uint32_t c )
{
	uint8_t r = (uint8_t)( ( c >> 24 ) & 0xFF );
	uint8_t g = (uint8_t)( ( c >> 16 ) & 0xFF );
	uint8_t b = (uint8_t)( ( c >> 8 ) & 0xFF );
	uint8_t a = (uint8_t)( c & 0xFF );

	return clr_byte( r, g, b, a );
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

// hue = [0, 360]  saturation = [0.0f, 1.0f]  value = [0.0f, 1.0f]
Color clr_hsv( int hue, float saturation, float value )
{
	Color c;
	c.a = 1.0f;

	while( hue > 360 ) hue -= 360;
	while( hue < 0 ) hue += 360;

	if( value == 0.0f ) {
		c.r = c.g = c.b = 0.0f;
	} else {
		
		float chroma = value * saturation;
		float hp = hue / 60.0f;
		float x = chroma * ( 1.0f - SDL_fabsf( SDL_fmodf( hp, 2.0f ) - 1.0f ) );

		if( FLT_BETWEEN( hp, 0.0f, 1.0f ) ) {
			c.r = chroma;
			c.g = x;
			c.b = 0.0f;
		} else if( FLT_BETWEEN( hp, 1.0f, 2.0f ) ) {
			c.r = x;
			c.g = chroma;
			c.b = 0.0f;
		} else if( FLT_BETWEEN( hp, 2.0f, 3.0f ) ) {
			c.r = 0.0f;
			c.g = chroma;
			c.b = x;
		} else if( FLT_BETWEEN( hp, 3.0f, 4.0f ) ) {
			c.r = 0.0f;
			c.g = x;
			c.b = chroma;
		} else if( FLT_BETWEEN( hp, 4.0f, 5.0f ) ) {
			c.r = x;
			c.g = 0.0f;
			c.b = chroma;
		} else if( FLT_BETWEEN( hp, 5.0f, 6.0f ) ) {
			c.r = chroma;
			c.g = 0.0f;
			c.b = x;
		} else {
			// dunno, just make it black
			c.r = c.g = c.b = 0.0f;
		}

		float m = value - chroma;
		c.r += m;
		c.g += m;
		c.b += m;
	}

	return c;
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