#include "geomTrail.h"

#include "../Graphics/triRendering.h"
#include "../Utils/stretchyBuffer.h"
#include "../System/systems.h"
#include "../Math/mathUtil.h"
#include "../Graphics/images.h"
#include "../Graphics/graphics.h"

typedef struct {
	Vector2 pos;
	//float timeLeft;

	float prevTimeLeft;
	float currTimeLeft;
} GeoTrailPoint;

typedef struct {
	bool inUse;
	GeomTrailWidthFunc widthFunc;
	GeomTrailColorFunc colorFunc;
	float baseWidth;
	float time;
	float minVertDistSqrd;

	Vector2 currOriginPos;
	Vector2 futureOriginPos;

	int img;
	GeoTrailPoint* sbTrailPoints;
	int8_t depth;
	uint32_t camFlags;
} GeomTrail;

static float defaultWidthFunc( float t )
{
	return 1.0f;
}

static void defaultColorFunc( float t, Color* outColor )
{
	(*outColor) = CLR_WHITE;
}

static GeomTrail* sbGeomTrails = NULL;
static int geomTrailSystem = -1;
static float lastPhysicsTick = 0.0f;
static float amtTickElapsed = 0.0f;

typedef struct {
	Vector2 pos;
	Vector2 norm;
	float dist;
} GeomTrailRenderEntry;

typedef struct {
	Vector2 pos;
	float timeLeft;
} WorkingGeoTrailPoint;

static WorkingGeoTrailPoint* sbWorkingPoints = NULL;

static void addNewTrailPoint( GeomTrail* trail, Vector2* pos )
{
	assert( trail != NULL );

	GeoTrailPoint* newPt = sb_Add( trail->sbTrailPoints, 1 );
	newPt->pos = (*pos);
	newPt->currTimeLeft = trail->time + ( lastPhysicsTick * amtTickElapsed );
	newPt->prevTimeLeft = trail->time + ( lastPhysicsTick * amtTickElapsed );
}

static void swap( void )
{
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {

		sbGeomTrails[i].currOriginPos = sbGeomTrails[i].futureOriginPos;

		// point list is empty, or am far enough away from the last placed point
		if( ( sb_Count( sbGeomTrails[i].sbTrailPoints ) <= 0 ) ||
			vec2_DistSqrd( &(sbGeomTrails[i].currOriginPos), &( sb_Last( sbGeomTrails[i].sbTrailPoints ).pos ) ) > ( sbGeomTrails[i].minVertDistSqrd ) ) {

			addNewTrailPoint( &(sbGeomTrails[i]), &(sbGeomTrails[i].currOriginPos ) );
		}
	}
}

static void drawGeomTrailTris( GeomTrail* trail, float t )
{
	assert( trail != NULL );
	
	// generate the list of points to use based on the curretn geom trail, assign the time to currTimeLeft
	sb_Clear( sbWorkingPoints );
	sb_Add( sbWorkingPoints, sb_Count( trail->sbTrailPoints ) );

	for( size_t i = 0; i < sb_Count( trail->sbTrailPoints ); ++i ) {
		sbWorkingPoints[i].pos = trail->sbTrailPoints[i].pos;
		sbWorkingPoints[i].timeLeft = lerp( trail->sbTrailPoints[i].prevTimeLeft, trail->sbTrailPoints[i].currTimeLeft, t );
	}

	// remove the dead points
	// TODO: work this into the loop above
	for( size_t i = 1; i < sb_Count( sbWorkingPoints ); ++i ) {
		if( sbWorkingPoints[i].timeLeft <= 0.0f ) {
			sb_Remove( sbWorkingPoints, i - 1 );
			--i;
		}
	}

	// now generate the trail
	GLuint textureID;
	img_GetTextureID( trail->img, &textureID );

	Vector2 currPos;
	Vector2 nextPos;
	Vector2 prevPos = VEC2_ZERO;
	float nextTimeLeft = 0.0f;

	Vector2 originPos;
	vec2_Lerp( &(trail->currOriginPos), &(trail->futureOriginPos), t, &originPos );

	size_t c = sb_Count( sbWorkingPoints ) + 1;
	GeomTrailRenderEntry* trailRenderGeom = mem_Allocate( sizeof( GeomTrailRenderEntry ) * c );

	float totalDist = 0.0f;
	for( size_t i = 0; i <= sb_Count( sbWorkingPoints ); ++i ) {
		// get next
		if( i == ( sb_Count( sbWorkingPoints ) - 1 ) ) {
			nextPos = originPos;
			nextTimeLeft = 0.0f;
		} else {
			nextPos = sbWorkingPoints[i+1].pos;
			nextTimeLeft = sbWorkingPoints[i+1].timeLeft;
		}

		// get current
		if( i < sb_Count( sbWorkingPoints ) ) {
			currPos = sbWorkingPoints[i].pos;
		} else {
			currPos = originPos;
		}

		// recalculate the current pos based on the time passed
		if( i < sb_Count( sbWorkingPoints ) ) {
			if( sbWorkingPoints[i].timeLeft < 0.0f ) {
				vec2_Lerp( &nextPos, &currPos, ( nextTimeLeft / ( nextTimeLeft - sbWorkingPoints[i].timeLeft ) ), &currPos );
			}
		}

		// setup the normal for the current point
		Vector2 norm = VEC2_ZERO;
		
		if( i == 0 ) {
			// first, only use next
			Vector2 dir;
			vec2_Subtract( &nextPos, &currPos, &dir );
			vec2_Normalize( &dir );
			norm.x = -dir.y;
			norm.y = dir.x;
		} else if( i == sb_Count( sbWorkingPoints ) ) {
			// last, only use prev
			Vector2 prevDir;
			vec2_Subtract( &currPos, &prevPos, &prevDir );
			float dist = vec2_Mag( &prevDir );
			totalDist += dist;
			vec2_Scale( &prevDir, ( 1.0f / dist ), &prevDir );

			norm.x = -prevDir.y;
			norm.y = prevDir.x;
		} else {
			// use both, need an average of the normals
			Vector2 nextDir;
			vec2_Subtract( &nextPos, &currPos, &nextDir );
			vec2_Normalize( &nextDir );

			Vector2 prevDir;
			vec2_Subtract( &currPos, &prevPos, &prevDir );
			float dist = vec2_Mag( &prevDir );
			totalDist += dist;
			vec2_Scale( &prevDir, ( 1.0f / dist ), &prevDir );

			Vector2 avgDir;
			vec2_Lerp( &nextDir, &prevDir, 0.5f, &avgDir );
			vec2_Normalize( &avgDir );

			norm.x = -avgDir.y;
			norm.y = avgDir.x;
		}

		trailRenderGeom[i].pos = currPos;
		trailRenderGeom[i].norm = norm;
		trailRenderGeom[i].dist = totalDist;

		prevPos = currPos;
	}

	// draw the generated points
	for( size_t i = 0; i < sb_Count( sbWorkingPoints ); ++i ) {
		TriVert vert0;
		TriVert vert1;
		TriVert vert2;
		TriVert vert3;

		float t0 = trailRenderGeom[i].dist / totalDist;
		float t1 = trailRenderGeom[i+1].dist / totalDist;

		
		float halfWidth0 = trail->widthFunc( t0 ) * trail->baseWidth / 2.0f;
		float halfWidth1 = trail->widthFunc( t1 ) * trail->baseWidth / 2.0f;

		vec2_AddScaled( &( trailRenderGeom[i].pos ), &( trailRenderGeom[i].norm ), halfWidth0, &(vert0.pos) );
		vec2_AddScaled( &( trailRenderGeom[i].pos ), &( trailRenderGeom[i].norm ), -halfWidth0, &(vert1.pos) );
		vec2_AddScaled( &( trailRenderGeom[i+1].pos ), &( trailRenderGeom[i+1].norm ), halfWidth1, &(vert2.pos) );
		vec2_AddScaled( &( trailRenderGeom[i+1].pos ), &( trailRenderGeom[i+1].norm ), -halfWidth1, &(vert3.pos) );

		vert0.uv = vec2( t0, 1.0f );
		vert1.uv = vec2( t0, 0.0f );
		vert2.uv = vec2( t1, 1.0f );
		vert3.uv = vec2( t1, 0.0f );

		Color color0;
		Color color1;
		trail->colorFunc( t0, &color0 );
		trail->colorFunc( t1, &color1 );
		vert0.col = color0;
		vert1.col = color0;
		vert2.col = color1;
		vert3.col = color1;

		triRenderer_Add( vert0, vert1, vert2, ST_DEFAULT, textureID, 0, trail->camFlags, trail->depth, 1 );
		triRenderer_Add( vert1, vert3, vert2, ST_DEFAULT, textureID, 0, trail->camFlags, trail->depth, 1 );
	}

	mem_Release( trailRenderGeom );
	sb_Release( sbWorkingPoints );
}

static void drawAllGeomTrails( float t )
{
	amtTickElapsed = t;
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		if( !sbGeomTrails[i].inUse ) continue;
		drawGeomTrailTris( &(sbGeomTrails[i]), t );
	}
}

void geomTrail_ShutDown( void )
{
	sb_Release( sbWorkingPoints );
	gfx_RemoveDrawTrisFunc( drawAllGeomTrails );
	gfx_RemoveClearCommand( swap );
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		sb_Release( sbGeomTrails[i].sbTrailPoints );
	}
	sb_Release( sbGeomTrails );

	sys_UnRegister( geomTrailSystem );
}

int geomTrail_Create( GeomTrailWidthFunc widthFunc, GeomTrailColorFunc colorFunc, float baseWidth, float time, float minVertDist, int img,
	int8_t depth, uint32_t camFlags )
{
	GeomTrail* trail;
	int idx = -1;
	for( size_t i = 0; ( i < sb_Count( sbGeomTrails ) ) && ( idx < 0 ); ++i ) {
		if( !sbGeomTrails[i].inUse ) {
			idx = (int)i;
		}
	}
	if( idx == SIZE_MAX ) {
		idx = (int)sb_Count( sbGeomTrails );
		trail = sb_Add( sbGeomTrails, 1 );
	} else {
		trail = &(sbGeomTrails[idx]);
	}
	
	trail->inUse = true;

	if( widthFunc == NULL ) {
		trail->widthFunc = defaultWidthFunc;
	} else {
		trail->widthFunc = widthFunc;
	}

	if( colorFunc == NULL ) {
		trail->colorFunc = defaultColorFunc;
	} else {
		trail->colorFunc = colorFunc;
	}

	trail->baseWidth = baseWidth;
	trail->time = time;
	trail->minVertDistSqrd = ( minVertDist * minVertDist );
	trail->currOriginPos = trail->futureOriginPos = VEC2_ZERO;
	trail->img = img;
	trail->sbTrailPoints = NULL;
	trail->depth = depth;
	trail->camFlags = camFlags;

	return idx;
}

void geomTrail_Destroy( int idx )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	sb_Release( sbGeomTrails[idx].sbTrailPoints );
	sbGeomTrails[idx].inUse = false;
}

void geomTrail_SetOriginPoint( int idx, Vector2* pos )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	sbGeomTrails[idx].futureOriginPos = (*pos);
}

static void trailPhysicsTick( GeomTrail* trail, float dt )
{
	for( size_t i = 0; i < sb_Count( trail->sbTrailPoints ); ++i ) {
		trail->sbTrailPoints[i].prevTimeLeft = trail->sbTrailPoints[i].currTimeLeft;
		trail->sbTrailPoints[i].currTimeLeft -= dt;

		//trail->sbTrailPoints[i].timeLeft -= dt;
		// we need the last point that died off in order to correctly move the end point of the trial
		if( ( i > 0 ) && ( trail->sbTrailPoints[i].prevTimeLeft <= 0.0f ) ) {
			sb_Remove( trail->sbTrailPoints, i - 1 );
			--i;
		}
	}
}

static void physicsTick( float dt )
{
	lastPhysicsTick = dt;
	amtTickElapsed = 0.0f;

	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		if( !sbGeomTrails[i].inUse ) continue;
		trailPhysicsTick( &(sbGeomTrails[i]), dt );
	}
}

void geomTrail_Init( void )
{
	gfx_AddDrawTrisFunc( drawAllGeomTrails );
	gfx_AddClearCommand( swap );

	sbGeomTrails = NULL;
	sbWorkingPoints = NULL;
	
	geomTrailSystem = sys_Register( NULL, NULL, NULL, physicsTick );
}

// returns a stretchy buffer, goes from the oldest point to the current origin
Vector2* geomTrail_GetPoints( int idx )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	GeomTrail* trail = &(sbGeomTrails[idx]);

	// generate a list of Vector2 that can be used for things
	Vector2* sbPts = NULL;

	sb_Clear( sbWorkingPoints );
	sb_Add( sbWorkingPoints, sb_Count( trail->sbTrailPoints ) );

	for( size_t i = 0; i < sb_Count( trail->sbTrailPoints ); ++i ) {
		sbWorkingPoints[i].pos = trail->sbTrailPoints[i].pos;
		sbWorkingPoints[i].timeLeft = lerp( trail->sbTrailPoints[i].prevTimeLeft, trail->sbTrailPoints[i].currTimeLeft, amtTickElapsed );
	}

	Vector2 originPos;
	vec2_Lerp( &(trail->currOriginPos), &(trail->futureOriginPos), amtTickElapsed, &originPos );

	// remove the dead points
	// TODO: work this into the loop above
	for( size_t i = 1; i < sb_Count( sbWorkingPoints ); ++i ) {
		if( sbWorkingPoints[i].timeLeft <= 0.0f ) {
			sb_Remove( sbWorkingPoints, i - 1 );
			--i;
		}
	}

	Vector2 currPos;
	Vector2 nextPos;
	float nextTimeLeft = 0.0f;

	for( size_t i = 0; i <= sb_Count( sbWorkingPoints ); ++i ) {
		// get next
		if( i == ( sb_Count( sbWorkingPoints ) - 1 ) ) {
			nextPos = originPos;
			nextTimeLeft = 0.0f;
		} else {
			nextPos = sbWorkingPoints[i+1].pos;
			nextTimeLeft = sbWorkingPoints[i+1].timeLeft;
		}

		// get current
		if( i < sb_Count( sbWorkingPoints ) ) {
			currPos = sbWorkingPoints[i].pos;
		} else {
			currPos = originPos;
		}

		// recalculate the current pos based on the time passed
		if( i < sb_Count( sbWorkingPoints ) ) {
			if( sbWorkingPoints[i].timeLeft < 0.0f ) {
				vec2_Lerp( &nextPos, &currPos, ( nextTimeLeft / ( nextTimeLeft - sbWorkingPoints[i].timeLeft ) ), &currPos );
			}
		}

		sb_Push( sbPts, currPos );
	}

	return sbPts;
}