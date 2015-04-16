#include "color.h"
#include <assert.h>
#include "../Math/mathUtil.h"

Color* col_Lerp( const Color* from, const Color* to, float t, Color* out )
{
	assert( from );
	assert( to );
	assert( out );

	out->r = lerp( from->r, to->r, t );
	out->g = lerp( from->g, to->g, t );
	out->b = lerp( from->b, to->b, t );
	out->a = lerp( from->a, to->a, t );

	return out;
}