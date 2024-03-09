#ifndef NUKLEAR_HEADER_H
#define NUKLEAR_HEADER_H

// so we always have matching defines

#include <SDL_assert.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO 1
//#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_VARARGS
//#define NK_BUTTON_TRIGGER_ON_RELEASE
#define NK_ASSERT(x) SDL_assert(x)

// some warnings in their code, assume they know what they're doing until proven otherwise
#pragma warning( push )
#pragma warning( disable : 4701 )
#include "Others/nuklear.h"
#pragma warning( pop )

#endif