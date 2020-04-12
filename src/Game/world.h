#ifndef WORLD_H
#define WORLD_H

#include "Math/vector2.h"

// for right now we're just storing the world dimensions here
void world_SetSize( int width, int height );
void world_GetSize( Vector2* outVec );

#endif
