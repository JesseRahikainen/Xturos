#include "geomTrail.h"

#include "Graphics/triRendering.h"
#include "Utils/stretchyBuffer.h"
#include "System/systems.h"
#include "Math/mathUtil.h"
#include "Graphics/images.h"
#include "Graphics/graphics.h"
#include "System/platformLog.h"
#include "Graphics/Platform/graphicsPlatform.h"
#include "Utils/helpers.h"

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
	float limit; // if timeBased == true then limit is the time, otherwise it's distance
	float minVertDist;
	bool timeBased;

	size_t distMaxNodes; // if timeBased == false then this is the maximum number of nodes we should store

	Vector2 currOriginPos;
	Vector2 futureOriginPos;

	int img;
	GeoTrailPoint* sbTrailPoints;
	int8_t depth;
	uint32_t camFlags;

	bool debug;
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
static float timeScale = 1.0f;

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

static void addNewTrailPoint( GeomTrail* trail, Vector2* pos, float time )
{
	assert( trail != NULL );

	GeoTrailPoint* newPt = sb_Add( trail->sbTrailPoints, 1 );
	newPt->pos = ( *pos );
	newPt->currTimeLeft = time;
	newPt->prevTimeLeft = time;
}

static void swap( void )
{
//llog( LOG_DEBUG, "swap" );
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {

//llog( LOG_DEBUG, "curr: <%.2f, %.2f>   future: <%.2f, %.2f>", sbGeomTrails[i].currOriginPos.x, sbGeomTrails[i].currOriginPos.y, sbGeomTrails[i].futureOriginPos.x, sbGeomTrails[i].futureOriginPos.x );
		sbGeomTrails[i].currOriginPos = sbGeomTrails[i].futureOriginPos;

		// if there are no points then create the first one
		if( sb_Count( sbGeomTrails[i].sbTrailPoints ) <= 0 ) {
			addNewTrailPoint( &( sbGeomTrails[i] ), &( sbGeomTrails[i].currOriginPos ), sbGeomTrails[i].limit + ( lastPhysicsTick * amtTickElapsed ) );
		}

		// if the position is far enough away from the last placed point then create new points along the line
		Vector2 origLastPos = sb_Last( sbGeomTrails[i].sbTrailPoints ).pos;
		if( vec2_DistSqrd( &( sbGeomTrails[i].currOriginPos ), &origLastPos ) > ( sbGeomTrails[i].minVertDist * sbGeomTrails[i].minVertDist ) ) {
			Vector2 diff;
			vec2_Subtract( &( sbGeomTrails[i].currOriginPos ), &origLastPos, &diff );
			float dist = vec2_Normalize( &diff );
			float startTime = sbGeomTrails[i].limit;
			float endTime = sbGeomTrails[i].limit + ( lastPhysicsTick * amtTickElapsed );

			float currDist = sbGeomTrails[i].minVertDist;
			while( currDist <= dist ) {
				Vector2 newPos;
				vec2_AddScaled( &origLastPos, &diff, currDist, &newPos );
				addNewTrailPoint( &( sbGeomTrails[i] ), &newPos, lerp( startTime, endTime, currDist / dist ) );
				currDist += sbGeomTrails[i].minVertDist;
			}//*/
		}//*/

		// for distance based paths remove any extra points that aren't needed
		if( !sbGeomTrails[i].timeBased ) {
			while( sb_Count( sbGeomTrails[i].sbTrailPoints ) > sbGeomTrails[i].distMaxNodes ) {
				sb_Remove( sbGeomTrails[i].sbTrailPoints, 0 );
			}
		}
	}
}

static void drawGeomTrailPoints( GeomTrail* trail, GeomTrailRenderEntry* trailRenderGeom, size_t renderCount, float totalDist )
{
	PlatformTexture texture;
	img_GetTextureID( trail->img, &texture );

	if( renderCount == 0 ) return;

	//if( trail->debug ) llog( LOG_DEBUG, "******" );
	for( size_t i = 0; i < renderCount - 1; ++i ) {
		TriVert vert0;
		TriVert vert1;
		TriVert vert2;
		TriVert vert3;
		//if( trail->debug) llog( LOG_DEBUG, " %.2f, %.2f -> %.2f, %.2f", trailRenderGeom[i].pos.x, trailRenderGeom[i].pos.y, trailRenderGeom[i + 1].pos.x, trailRenderGeom[i + 1].pos.y );

		float t0 = trailRenderGeom[i].dist / totalDist;
		float t1 = trailRenderGeom[i + 1].dist / totalDist;


		float halfWidth0 = trail->widthFunc( t0 ) * trail->baseWidth / 2.0f;
		float halfWidth1 = trail->widthFunc( t1 ) * trail->baseWidth / 2.0f;

		vec2_AddScaled( &( trailRenderGeom[i].pos ), &( trailRenderGeom[i].norm ), halfWidth0, &( vert0.pos ) );
		vec2_AddScaled( &( trailRenderGeom[i].pos ), &( trailRenderGeom[i].norm ), -halfWidth0, &( vert1.pos ) );
		vec2_AddScaled( &( trailRenderGeom[i + 1].pos ), &( trailRenderGeom[i + 1].norm ), halfWidth1, &( vert2.pos ) );
		vec2_AddScaled( &( trailRenderGeom[i + 1].pos ), &( trailRenderGeom[i + 1].norm ), -halfWidth1, &( vert3.pos ) );

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

		//if( trail->debug ) llog( LOG_DEBUG, " 0: %.2f, %.2f  -  1: %.2f, %.2f  -  2: %.2f, %.2f  -  3: %.2f, %.2f", vert0.pos.x, vert0.pos.y, vert1.pos.x, vert1.pos.y, vert2.pos.x, vert2.pos.y, vert3.pos.x, vert3.pos.y );
		triRenderer_Add( vert0, vert1, vert2, ST_DEFAULT, texture, gfxPlatform_GetDefaultPlatformTexture( ), 0.0f, -1, trail->camFlags, trail->depth, TT_TRANSPARENT );
		triRenderer_Add( vert1, vert3, vert2, ST_DEFAULT, texture, gfxPlatform_GetDefaultPlatformTexture( ), 0.0f, -1, trail->camFlags, trail->depth, TT_TRANSPARENT );
	}
}

static void drawGeomTrailTris_Time( GeomTrail* trail, float t )
{
	assert( trail != NULL );
	
	// generate the list of points to use based on the current geom trail, assign the time to currTimeLeft
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

	//llog( LOG_DEBUG, "trailSize: %i", sb_Count( sbWorkingPoints ) );
	// draw the generated points
	drawGeomTrailPoints( trail, trailRenderGeom, sb_Count( sbWorkingPoints ), totalDist );

	mem_Release( trailRenderGeom );
}

static void drawGeomTrailTris_Distance( GeomTrail* trail, float t )
{
	assert( trail != NULL );
	// generate the list of points to use based on the current geom trail, assign the time to currTimeLeft
	sb_Clear( sbWorkingPoints );
	sb_Add( sbWorkingPoints, sb_Count( trail->sbTrailPoints ) );

	// remember, the final point is the one that's closest to the origin point
	//  we'll need to get the full length of the path, from there we'll need to see
	//  if it's greater than the total length allowed, then we'll need to cut off
	//  the end bit enough to limit the length
	for( size_t i = 0; i < sb_Count( trail->sbTrailPoints ); ++i ) {
		sbWorkingPoints[i].pos = trail->sbTrailPoints[i].pos;
	}
	Vector2 originPos;
	vec2_Lerp( &( trail->currOriginPos ), &( trail->futureOriginPos ), t, &originPos );
	Vector2 lastPos = sb_Last( sbWorkingPoints ).pos;
	float endDist = vec2_Dist( &originPos, &lastPos );

	// now generate the trail
	Vector2 currPos;
	Vector2 nextPos;
	Vector2 prevPos = VEC2_ZERO;
	float nextTimeLeft = 0.0f;

	vec2_Lerp( &( trail->currOriginPos ), &( trail->futureOriginPos ), t, &originPos );

	size_t first = 0;

	// trail->limit is the total possible length of the trail
	// trail->distMaxNodes is the number of nodes on the trail until max distance is hit
	// trail->minVertDist is how much distance there is between nodes
	// end dist is the distance between the origin pos and the node closest to it
	// so, assuming we
	float remainder = ( trail->limit - ( ( trail->distMaxNodes - 2 ) * trail->minVertDist ) ) - endDist;
	if( ( remainder < 0.0f ) && ( sb_Count( sbWorkingPoints ) >= trail->distMaxNodes ) ) {
		first = 1;
		remainder = trail->minVertDist + remainder;
	}

	size_t c = sb_Count( sbWorkingPoints ) + 1 - first;
	GeomTrailRenderEntry* trailRenderGeom = mem_Allocate( sizeof( GeomTrailRenderEntry ) * c );

	//if( trail->debug ) llog( LOG_DEBUG, "======= %i  %.2f   %.2f", c, remainder, endDist );
	//if( trail->debug ) llog( LOG_DEBUG, " origin: %.2f, %.2f", originPos.x, originPos.y );
	float totalDist = 0.0f;
	for( size_t i = first; i <= sb_Count( sbWorkingPoints ); ++i ) {
		// get next
		if( i == ( sb_Count( sbWorkingPoints ) - 1 ) ) {
			nextPos = originPos;
		} else {
			nextPos = sbWorkingPoints[i + 1].pos;
		}
		
		// get current
		if( i < sb_Count( sbWorkingPoints ) ) {
			currPos = sbWorkingPoints[i].pos;
		} else {
			currPos = originPos;
		}

		// recalculate the current pos based on the how far the origin is away from the newest point
		if( i == first ) {
			Vector2 diff;
			vec2_Subtract( &currPos, &nextPos, &diff );
			vec2_Normalize( &diff );
			vec2_AddScaled( &nextPos, &diff, remainder, &currPos );
		}

		// setup the normal for the current point
		Vector2 norm = VEC2_ZERO;

		if( i == first ) {
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

		//if( trail->debug ) llog( LOG_DEBUG, " - %.2f, %.2f", currPos.x, currPos.y );

		size_t idx = i - first;
		trailRenderGeom[idx].pos = currPos;
		trailRenderGeom[idx].norm = norm;
		trailRenderGeom[idx].dist = totalDist;

		prevPos = currPos;
	}

	// draw the generated points
	drawGeomTrailPoints( trail, trailRenderGeom, c, totalDist );

	mem_Release( trailRenderGeom );
}

static void drawAllGeomTrails( float t )
{
	amtTickElapsed = t;
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		if( !sbGeomTrails[i].inUse ) continue;
		if( sbGeomTrails[i].timeBased ) {
			drawGeomTrailTris_Time( &( sbGeomTrails[i] ), t );
		} else {
			drawGeomTrailTris_Distance( &( sbGeomTrails[i] ), t );
		}
	}
}

void geomTrail_ShutDown( void )
{
	sb_Release( sbWorkingPoints );
	//gfx_RemoveDrawTrisFunc( drawAllGeomTrails );
	gfx_RemoveClearCommand( swap );
	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		sb_Release( sbGeomTrails[i].sbTrailPoints );
	}
	sb_Release( sbGeomTrails );

	sys_UnRegister( geomTrailSystem );
	geomTrailSystem = -1;
}

int geomTrail_Create( GeomTrailWidthFunc widthFunc, GeomTrailColorFunc colorFunc, float baseWidth, float limit, float minVertDist, int img,
	int8_t depth, uint32_t camFlags, bool useTime )
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
		trail = &( sbGeomTrails[idx] );
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
	trail->limit = limit;
	trail->timeBased = useTime;
	trail->minVertDist = minVertDist;
	trail->currOriginPos = trail->futureOriginPos = VEC2_ZERO;
	trail->img = img;
	trail->sbTrailPoints = NULL;
	trail->depth = depth;
	trail->camFlags = camFlags;
	trail->debug = false;

	trail->distMaxNodes = 0 ;
	if( !useTime ) {
		trail->distMaxNodes = ( (size_t)SDL_ceilf( limit / minVertDist ) ) + 1;
		sb_Reserve( trail->sbTrailPoints, trail->distMaxNodes );
	}

	return idx;
}

// adjusts how fast the life time of each point in the geom trail is reduced
void geomTrail_SetTimeScale( float newTimeScale )
{
	timeScale = newTimeScale;
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
	//llog( LOG_DEBUG, "setting origin: %.2f, %.2f", sbGeomTrails[idx].futureOriginPos.x, sbGeomTrails[idx].futureOriginPos.y );
}

// sets the current points from which the trail should originate, does not create the new points
void geomTrail_ClampOriginPoint( int idx, Vector2* pos )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	sbGeomTrails[idx].currOriginPos = sbGeomTrails[idx].futureOriginPos = ( *pos );
}

static void trailPhysicsTick_Time( GeomTrail* trail, float dt )
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

void geomTrail_DumpTailData( int idx, char* fileName )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	GeoTrailPoint* sbTrail = sbGeomTrails[idx].sbTrailPoints;

	// testing, dump out session information
	char* fullPath = getSavePath( fileName );
	llog( LOG_DEBUG, "dumping tail data %i to: %s", idx, fullPath );

	SDL_RWops* dataFile = SDL_RWFromFile( fullPath, "w" );

	if( dataFile == NULL ) {
		llog( LOG_DEBUG, "Unable to open file: %s", SDL_GetError( ) );
		goto pathcleanup;
	}

	char message[256];
	size_t strLen;

	memset( message, 0, 256 );
	SDL_snprintf( message, 255, "count: %i\r\n", sb_Count( sbTrail ) );
	strLen = SDL_strlen( message );
	SDL_RWwrite( dataFile, message, 1, strLen );

	memset( message, 0, 256 );
	SDL_snprintf( message, 255, "prevTimeLeft, currTimeLeft, yOffset\r\n" );
	strLen = SDL_strlen( message );
	SDL_RWwrite( dataFile, message, 1, strLen );

	for( size_t i = 0; i < sb_Count( sbTrail ); ++i ) {
		memset( message, 0, 256 );
		SDL_snprintf( message, 255, "%f, %f, %f\r\n", sbTrail[i].prevTimeLeft, sbTrail[i].currTimeLeft, sbTrail[i].pos.y );
		strLen = SDL_strlen( message );
		SDL_RWwrite( dataFile, message, 1, strLen );
	}

	SDL_RWclose( dataFile );

pathcleanup:
	mem_Release( fullPath );
}

static void physicsTick( float dt )
{
	lastPhysicsTick = dt * timeScale;
	amtTickElapsed = 0.0f;

	if( timeScale <= 0.0f ) {
		return;
	}

	for( size_t i = 0; i < sb_Count( sbGeomTrails ); ++i ) {
		if( !sbGeomTrails[i].inUse ) continue;
		if( sbGeomTrails[i].timeBased ) {
			trailPhysicsTick_Time( &( sbGeomTrails[i] ), dt * timeScale );
		}
	}
}

void geomTrail_Init( void )
{
	if( geomTrailSystem != -1 ) return;

	gfx_AddDrawTrisFunc( drawAllGeomTrails );
	gfx_AddClearCommand( swap );

	sbGeomTrails = NULL;
	sbWorkingPoints = NULL;
	
	geomTrailSystem = sys_Register( NULL, NULL, NULL, physicsTick );
}

//******************************************************************************
static Vector2* getPoints_TimeBased( int idx, unsigned int skip )
{
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

	unsigned int skipCount = 0;
	for( size_t i = 0; i <= sb_Count( sbWorkingPoints ); ++i ) {
		if( ( skipCount != 0 ) && ( i != sb_Count( sbWorkingPoints ) ) ) {
			--skipCount;
			continue;
		}

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

		skipCount = skip;
	}

	return sbPts;
}

static Vector2* getPoints_DistanceBased( int idx, unsigned int skip )
{
	GeomTrail* trail = &( sbGeomTrails[idx] );

	// generate a list of Vector2 that can be used for things
	Vector2* sbPts = NULL;

	sb_Clear( sbWorkingPoints );
	sb_Add( sbWorkingPoints, sb_Count( trail->sbTrailPoints ) );

	for( size_t i = 0; i < sb_Count( trail->sbTrailPoints ); ++i ) {
		sbWorkingPoints[i].pos = trail->sbTrailPoints[i].pos;
	}

	Vector2 originPos;
	vec2_Lerp( &( trail->currOriginPos ), &( trail->futureOriginPos ), amtTickElapsed, &originPos );

	// all segments except the last one will be full length
	//  0 is the end
	Vector2 currPos;
	Vector2 nextPos;
	float endDist = vec2_Dist( &originPos, &( sb_Last( sbWorkingPoints ).pos ) );

	// if we have a negative remainder that means we need to eat some into the second to last
	//  segment and not display the last segment at all
	size_t first = 0;
	float remainder = ( trail->limit - ( ( trail->distMaxNodes - 2 ) * trail->minVertDist ) ) - endDist;
	if( ( remainder < 0.0f ) && ( sb_Count( sbWorkingPoints ) >= trail->distMaxNodes ) ) {
		first = 1;
		remainder = trail->minVertDist + remainder;
	}
	unsigned int skipCount = 0;
	for( size_t i = first; i <= sb_Count( sbWorkingPoints ); ++i ) {
		if( ( skipCount != 0 ) && ( i != sb_Count( sbWorkingPoints ) ) ) {
			--skipCount;
			continue;
		}

		// get next
		if( i == ( sb_Count( sbWorkingPoints ) - 1 ) ) {
			nextPos = originPos;
		} else {
			nextPos = sbWorkingPoints[i + 1].pos;
		}

		// get current
		if( i < sb_Count( sbWorkingPoints ) ) {
			currPos = sbWorkingPoints[i].pos;
		} else {
			currPos = originPos;
		}

		// if we're at the end of the paths then cut off some of the end
		if( i == first && ( sb_Count( sbWorkingPoints ) >= trail->distMaxNodes ) ) {
			Vector2 diff;
			vec2_Subtract( &currPos, &nextPos, &diff );
			vec2_Normalize( &diff );
			vec2_AddScaled( &nextPos, &diff, remainder, &currPos );
		}

		sb_Push( sbPts, currPos );

		skipCount = skip;
	}


	return sbPts;
}

// returns a stretchy buffer, goes from the oldest point to the current origin
Vector2* geomTrail_GetPoints( int idx, unsigned int skip )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	if( sbGeomTrails[idx].timeBased ) {
		return getPoints_TimeBased( idx, skip );
	} else {
		return getPoints_DistanceBased( idx, skip );
	}
}

void geomTrail_AdjustTiming( int idx, float start, float end )
{
	// go through each point and make it so the last one is at the end of it's life, the first is at the start, and the rest
	//  are between
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );
	assert( sbGeomTrails[idx].timeBased );

	GeomTrail* trail = &( sbGeomTrails[idx] );

	size_t numPoints = sb_Count( trail->sbTrailPoints );
	for( size_t i = 0; i < numPoints; ++i ) {
		float t = (float)i / (float)numPoints;
		trail->sbTrailPoints[i].currTimeLeft = trail->limit * lerp( start, end, t );
		trail->sbTrailPoints[i].prevTimeLeft = trail->limit * lerp( start, end, t );
	}
}

void geomTrail_SetStartingState( int idx, size_t numPoints, float* prevTimes, float* currTimes, float* yOffset )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	GeomTrail* trail = &( sbGeomTrails[idx] );

	sb_Clear( trail->sbTrailPoints );

	for( size_t i = 0; i < numPoints; ++i ) {
		GeoTrailPoint point;
		point.currTimeLeft = currTimes[i];
		point.prevTimeLeft = prevTimes[i];
		point.pos = trail->currOriginPos;
		point.pos.y -= yOffset[i];

		sb_Push( trail->sbTrailPoints, point );
	}
}

// creates a trail from startPoint to the origin pos
void geomTrail_SetInitialStateDist( int idx, Vector2 dir )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );
	assert( !sbGeomTrails[idx].timeBased );
	float dirMagSqrd = vec2_MagSqrd( &dir );
	assert( FLT_EQ( dirMagSqrd, 1.0f ) );

	sb_Clear( sbGeomTrails[idx].sbTrailPoints );

	Vector2 currPos;
	float currDist = sbGeomTrails[idx].minVertDist;
	do {
		vec2_AddScaled( &( sbGeomTrails[idx].currOriginPos ), &dir, currDist, &currPos );
		addNewTrailPoint( &( sbGeomTrails[idx] ), &currPos, 0.0f );
		currDist += sbGeomTrails[idx].minVertDist;
	} while( currDist < sbGeomTrails[idx].limit );
}

void geomTrail_SetDebugging( int idx, bool debug )
{
	assert( idx >= 0 );
	assert( idx < (int)sb_Count( sbGeomTrails ) );
	assert( sbGeomTrails[idx].inUse );

	sbGeomTrails[idx].debug = debug;
}