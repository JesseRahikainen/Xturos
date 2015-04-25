#ifndef COLOR_H
#define COLOR_H

#include <SDL_pixels.h>

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

Color* clr_Lerp( const Color* from, const Color* to, float t, Color* out );
SDL_Color clr_ToSDLColor( const Color* color );

#endif // inclusion guard