#ifndef GEOM_TRAIL_H
#define GEOM_TRAIL_H

#include "color.h"
#include "../Math/vector2.h"

typedef float(*GeomTrailWidthFunc)( float t );
typedef void(*GeomTrailColorFunc)( float t, Color* outColor );

// intialize the geomtrail, must be called before creating and allows drawing of the trails
void geomTrail_Init( void );

// shuts down and cleans up the geomTrail stuff
void geomTrail_ShutDown( void );

// creates a geom trail and returns an id to use for it
int geomTrail_Create( GeomTrailWidthFunc widthFunc, GeomTrailColorFunc colorFunc, float baseWidth, float time, float minVertDist, int img,
	int8_t depth, uint32_t camFlags );

// destroys the passed in geom trail
void geomTrail_Destroy( int id );

// sets the current points from which the trail should originate, also handles adding points to the trail
void geomTrail_SetOriginPoint( int id, Vector2* pos );

// gets the list of points that define the trail, returns a stretchy buffer
//  goes from the oldest point to the current origin
Vector2* geomTrail_GetPoints( int id );

#endif // inclusion guard