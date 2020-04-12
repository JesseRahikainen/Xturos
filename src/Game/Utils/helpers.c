#include "helpers.h"

#include "../Math/vector2.h"
#include "../Input/input.h"
#include "../System/platformLog.h"

void logMousePos( void )
{
	Vector2 pos;
	input_GetMousePosition( &pos );
	vec2_Dump( &pos, "Mouse pos" );
}