#ifndef COLOR_H
#define COLOR_H

#include <SDL_pixels.h>
#include <stdint.h>

/*
Standard 32-bit colors. Doesn't handle HDR or any fancy stuff.
*/

typedef struct {
	union {
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		float col[4];
	};
} Color;

static const Color CLR_BLACK = { 0.0f, 0.0f, 0.0f, 1.0f };
static const Color CLR_RED = { 1.0f, 0.0f, 0.0f, 1.0f };
static const Color CLR_GREEN = { 0.0f, 1.0f, 0.0f, 1.0f };
static const Color CLR_BLUE = { 0.0f, 0.0f, 1.0f, 1.0f };
static const Color CLR_YELLOW = { 1.0f, 1.0f, 0.0f, 1.0f };
static const Color CLR_MAGENTA = { 1.0f, 0.0f, 1.0f, 1.0f };
static const Color CLR_CYAN = { 0.0f, 1.0f, 1.0f, 1.0f };
static const Color CLR_WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
static const Color CLR_DARK_GREY = { 0.25f, 0.25f, 0.25f, 1.0f };
static const Color CLR_GREY = { 0.5f, 0.5f, 0.5f, 1.0f };
static const Color CLR_LIGHT_GREY = { 0.75f, 0.75f, 0.75f, 1.0f };

Color clr( float r, float g, float b, float a );
Color clr_byte( uint8_t r, uint8_t g, uint8_t b, uint8_t a );
Color clr_hex( uint32_t c );
SDL_Color clr_ToSDLColor( const Color* color );

Color* clr_Lerp( const Color* from, const Color* to, float t, Color* out );

// effects the alpha as well as the rgb
Color* clr_Scale( const Color* color, float scale, Color* out );
Color* clr_AddScaled( const Color* base, const Color* scaled, float scale, Color* out );

#endif // inclusion guard