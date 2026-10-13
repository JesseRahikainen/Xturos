#define NK_IMPLEMENTATION
#include "nuklearHeader.h"

char* nk_sdl_dtoa( char* str, double d )
{
    NK_ASSERT( str );
    if( !str ) return NULL;
    (void)SDL_snprintf( str, 99999, "%.17g", d );
    return str;
}