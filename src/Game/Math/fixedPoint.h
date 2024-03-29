#ifndef FIXED_POINT_H
#define FIXED_POINT_H

// primarily used to convert floating to fixed point for saving

#include <stdint.h>
#include <stdbool.h>
#include "Others/cmp.h"

typedef int32_t fixed32;

fixed32 f32_FromFloat( float f );
float f32_FromFixedPoint( fixed32 fp );
fixed32 f32_FromParts( int16_t whole, uint16_t fraction );

fixed32 f32_Add( fixed32 lhs, fixed32 rhs );
fixed32 f32_SaturatedAdd( fixed32 lhs, fixed32 rhs );
fixed32 f32_Subtract( fixed32 lhs, fixed32 rhs );
fixed32 f32_Multiply( fixed32 lhs, fixed32 rhs );
fixed32 f32_Divide( fixed32 lhs, fixed32 hrs );

bool f32_Serialize( cmp_ctx_t* cmp, fixed32 fp );
bool f32_Deserialize( cmp_ctx_t* cmp, fixed32* outFp );

#endif /* inclusion guard */