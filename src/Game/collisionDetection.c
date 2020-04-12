#include "collisionDetection.h"

#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <float.h>

#include "Graphics/debugRendering.h"
#include "Math/vector2.h"
#include "Math/mathUtil.h"
#include "Utils/stretchyBuffer.h"
#include "System/platformLog.h"

// raycasting intersection functions
int RayCastvAABBAxis( float rayStart, float dir, float boxMin, float boxMax, float* tMin, float* tMax )
{
	float ood;
	float t1, t2;
	float swap;

	if( fabsf( dir ) < 0.001f ) {
		/* ray parallel to axis, no hit unless origin within slab */
		if( ( rayStart < boxMin ) || ( rayStart > boxMax ) ) {
			return 0;
		}
	} else {
		/* compute intersection of the ray with the near and far line of the slab */
		ood = 1.0f / dir;
		t1 = ( boxMin - rayStart ) * ood;
		t2 = ( boxMax - rayStart ) * ood;

		/* make t1 the intersection with the near plane, t2 with the far plane */
		if( t1 > t2 ) {
			swap = t1;
			t1 = t2;
			t2 = swap;
		}

		/* compute intersection */
		(*tMin) = MAX( (*tMin), t1 );
		(*tMax) = MIN( (*tMax), t2 );

		if( (*tMin) > (*tMax) ) {
			return 0;
		}
	}

	return 1;
}

int LineAABBTest( Vector2* start, Vector2* end, ColliderAABB* aabb, float maxAllowedT, float minAllowedT, float* t, Vector2* pt )
{
	float tMin, tMax;

	tMin = minAllowedT;
	tMax = maxAllowedT;

	if( !RayCastvAABBAxis( start->v[0], ( end->v[0] - start->v[0] ),
							( aabb->center.v[0] - aabb->halfDim.v[0] ), ( aabb->center.v[0] + aabb->halfDim.v[0] ),
							&tMin, &tMax ) ) {
		return 0;
	}

	if( !RayCastvAABBAxis( start->v[1], ( end->v[1] - start->v[1] ),
							( aabb->center.v[1] - aabb->halfDim.v[1] ), ( aabb->center.v[1] + aabb->halfDim.v[1] ),
							&tMin, &tMax ) ) {
		return 0;
	}

	vec2_AddScaled( start, end, tMin, pt );
	(*t) = tMin;

	return 1;
}

int RayCastvAABB( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	return LineAABBTest( start, dir, &(collider->aabb), FLT_MAX, 0.0f, t, pt );
}

int RayCastvCircle( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	Vector2 startCircOrig;
	float a, b, c, discr;


	/* translate the sphere to the origin */
	vec2_Subtract( start, &( collider->circle.center ), &startCircOrig );
	b = 2.0f * 	vec2_DotProduct( &startCircOrig, dir );
	c = vec2_DotProduct( &startCircOrig, &startCircOrig ) - ( collider->circle.radius * collider->circle.radius );

	/* rays origin is outside the circle, and it's pointing away, so no chance of hitting */
	if( ( c > 0.0f ) && ( b > 0.0f ) ) {
		return 0;
	}

	a = vec2_DotProduct( dir, dir );
	discr = ( b * b ) - ( 4 * a * c );

	/* ray missed the circle */
	if( discr < 0.0f ) {
		return 0;
	}

	(*t) = ( -b - sqrtf( discr ) ) / ( 2 * a );

	/* if the ray starts inside the sphere */
	if( (*t) < 0.0f ) {
		(*t) = 0.0f;
	}

	vec2_AddScaled( start, dir, (*t), pt );

	return 1;
}

int RayCastvHalfSpace( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	// if the ray starts inside the half space we'll consider the collision at the rays origin

	float nDotA = vec2_DotProduct( &( collider->halfSpace.normal ), start );
	float nDotDir = vec2_DotProduct( &( collider->halfSpace.normal ), dir );
	(*t) = ( collider->halfSpace.d - nDotA ) / nDotDir;

	if( ( (*t) >= 0.0f ) && ( (*t) <= 1.0f ) ) {
		vec2_AddScaled( start, dir, (*t), pt );
		return 1;
	}

	return 0;
}

typedef int(*RayCastCheck)( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt );
RayCastCheck rayCastChecks[NUM_COLLIDER_TYPES] = { RayCastvAABB, RayCastvCircle, RayCastvHalfSpace };

// Standard Collision Functions
//  used if we don't want to ever handle collisions between these two types.
static int invalidCollision( Collider* primary, Collider* secondard, Vector2* outSeparation )
{
	llog( LOG_WARN, "Attempting to detect a collision between two collider types that has no valid detection method." );
	return 0;
}

static int AABBvHalfSpace( Collider* primary, Collider* secondary, Vector2* outSeparation )
{
	// just test all the corners, the deepest one determines the separation
	Vector2 corners[4];

	corners[0] = primary->aabb.center;
	corners[0].x += primary->aabb.halfDim.x;
	corners[0].y += primary->aabb.halfDim.y;

	corners[1] = primary->aabb.center;
	corners[1].x -= primary->aabb.halfDim.x;
	corners[1].y += primary->aabb.halfDim.y;

	corners[2] = primary->aabb.center;
	corners[2].x -= primary->aabb.halfDim.x;
	corners[2].y -= primary->aabb.halfDim.y;

	corners[3] = primary->aabb.center;
	corners[3].x += primary->aabb.halfDim.x;
	corners[3].y -= primary->aabb.halfDim.y;

	float deepest = FLT_MAX;
	for( int i = 0; i < 4; ++i ) {
		float dist = vec2_DotProduct( &( corners[i] ), &( secondary->halfSpace.normal ) ) - secondary->halfSpace.d;
		if( dist < deepest ) {
			deepest = dist;
		}
	}

	if( deepest >= 0.0f ) {
		return 0;
	}

	vec2_Scale( &( secondary->halfSpace.normal ), -deepest, outSeparation );
	return 1;
}

static int HalfSpacevAABB( Collider* primary, Collider* secondary, Vector2* outSeparation )
{
	if( AABBvHalfSpace( primary, secondary, outSeparation ) ) {
		outSeparation->x = -outSeparation->x;
		outSeparation->y = -outSeparation->y;
		return 1;
	}

	return 0;
}

static int CirclevHalfSpace( Collider* primary, Collider* secondary, Vector2* outSeparation )
{
	float dist = vec2_DotProduct( &( primary->circle.center ), &( secondary->halfSpace.normal ) ) - secondary->halfSpace.d;
	if( dist > primary->circle.radius ) {
		return 0;
	}

	vec2_Scale( &( secondary->halfSpace.normal ), ( primary->circle.radius - dist ), outSeparation );

	return 1;
}

static int HalfSpacevCircle( Collider* primary, Collider* secondary, Vector2* outSeparation )
{
	if( CirclevHalfSpace( primary, secondary, outSeparation ) ) {
		outSeparation->x = -outSeparation->x;
		outSeparation->y = -outSeparation->y;
		return 1;
	}

	return 0;
}

/*
Determines if the collider primary overlaps with fixed, and if they do how much primary has to be moved to stop colliding.
*/
static int AABBvAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 penetration;
	Vector2 sumDim;
	Vector2 diffDim;

	vec2_Add( &( primary->aabb.halfDim ), &( fixed->aabb.halfDim ), &sumDim );
	vec2_Subtract( &( primary->aabb.center ), &( fixed->aabb.center ), &diffDim );
	
	diffDim.v[0] = fabsf( diffDim.v[0] );
	diffDim.v[1] = fabsf( diffDim.v[1] );
	vec2_Subtract( &( sumDim ), &( diffDim ), &penetration );
	if( ( penetration.v[0] <= 0.0f ) || ( penetration.v[1] <= 0.0f ) ) {
		return 0;
	}

	/* collision on both axi, figure out how much we have to move to stop colliding */
	if( penetration.v[1] <= penetration.v[0] ) {
		outSeparation->v[0] = 0.0f;
		if( primary->aabb.center.v[1] < fixed->aabb.center.v[1] ) {
			outSeparation->v[1] = -penetration.v[1];
		} else {
			outSeparation->v[1] = penetration.v[1];
		}
	} else {
		if( primary->aabb.center.v[0] < fixed->aabb.center.v[0] ) {
			outSeparation->v[0] = -penetration.v[0];
		} else {
			outSeparation->v[0] = penetration.v[0];
		}
		outSeparation->v[1] = 0.0f;
	}

	return 1;
}

int CirclevAABB( Collider* primary, Collider* fixed, Vector2* outSeparation  )
{
	float xPenetration;
	float yPenetration;
	Vector2 diff;
	Vector2 corner;
	float sumDim;
	float diffLen;
	float separationLen;

	vec2_Subtract( &( primary->aabb.center ), & ( fixed->aabb.center ), &diff );

	/* first check to make sure collision is possible */
	sumDim = primary->circle.radius + fixed->aabb.halfDim.v[0];
	xPenetration = sumDim - fabsf( diff.v[0] );
	if( xPenetration <= 0 ) {
		return 0;
	}

	sumDim = primary->circle.radius + fixed->aabb.halfDim.v[1];
	yPenetration = sumDim - fabsf( diff.v[1] );
	if( yPenetration <= 0 ) {
		return 0;
	}

	/* if the circle is in one of the corner zones for the box then check against that corner
		that's all we should need to check */
	if( ( fabsf( diff.v[0] ) >= fixed->aabb.halfDim.v[0] ) && ( fabsf( diff.v[1] ) >= fixed->aabb.halfDim.v[1] ) ) {
		/* first figure out which corner the circle is closest to */
		if( diff.v[0] < 0.0f ) {
			corner.v[0] = fixed->aabb.center.v[0] - fixed->aabb.halfDim.v[0];
		} else {
			corner.v[0] = fixed->aabb.center.v[0] + fixed->aabb.halfDim.v[0];
		}

		if( diff.v[1] < 0.0f ) {
			corner.v[1] = fixed->aabb.center.v[1] - fixed->aabb.halfDim.v[1];
		} else {
			corner.v[1] = fixed->aabb.center.v[1] + fixed->aabb.halfDim.v[1];
		}

		vec2_Subtract( &( primary->circle.center ), &corner, &diff );

		/* check to see if the distance from the corner is smaller enough for a collision */
		if( vec2_MagSqrd( &diff ) >= ( primary->circle.radius * primary->circle.radius ) ) {
			return 0;
		}

		/* now calculate the actual separation amount needed */
		diffLen = vec2_Mag( &diff );
		separationLen = primary->circle.radius - diffLen;
		outSeparation->v[0] = ( diff.v[0] / diffLen ) * separationLen;
		outSeparation->v[1] = ( diff.v[1] / diffLen ) * separationLen;
	} else {
		/* otherwise just check like a normal the circle was an aabb with half lengths equal to it's radius */
		/* collision on both axi, figure out how much we have to move to stop colliding */
		if( yPenetration <= xPenetration ) {
			outSeparation->v[0] = 0.0f;
			if( primary->aabb.center.v[1] < fixed->aabb.center.v[1] ) {
				outSeparation->v[1] = -yPenetration;
			} else {
				outSeparation->v[1] = yPenetration;
			}
		} else {
			if( primary->aabb.center.v[0] < fixed->aabb.center.v[0] ) {
				outSeparation->v[0] = -xPenetration;
			} else {
				outSeparation->v[0] = xPenetration;
			}
			outSeparation->v[1] = 0.0f;
		}
	}

	return 1;
}

int AABBvCircle( Collider* primary, Collider* fixed, Vector2* outSeparation  )
{
	/* use existing code and just flip the separation */
	if( CirclevAABB( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return 1;
	}

	return 0;
}

int CirclevCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	float radiiSum;
	float dist;

	radiiSum = ( primary->circle.radius + fixed->circle.radius );
	radiiSum *= radiiSum;
	vec2_Subtract( &( primary->circle.center ), &( fixed->circle.center ), outSeparation );
	dist = vec2_MagSqrd( outSeparation );

	if( dist >= radiiSum ) {
		return 0;
	}

	/* lots of square roots, better way to do this? */
	radiiSum = sqrtf( radiiSum );
	dist = sqrtf( dist );
	vec2_Normalize( outSeparation );
	vec2_Scale( outSeparation, ( radiiSum - dist ), outSeparation );

	return 1;
}

int AABBvLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;

	if( LineAABBTest( &( fixed->lineSegment.posOne ), &( fixed->lineSegment.posTwo ), &(primary->aabb), 1.0f, 0.0f, &t, &collPt ) ) {
		// the line segment is fixed
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return 1;
	}

	return 0;
}

int LineSegmentvAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( AABBvLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return 1;
	}
	return 0;
}

int CirclevLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;
	Vector2 dir;
	vec2_Subtract( &( fixed->lineSegment.posTwo ), &( fixed->lineSegment.posOne ), &dir );
	float len = vec2_Normalize( &dir );

	int ret = RayCastvCircle( &( fixed->lineSegment.posOne ), &dir, primary, &t, &collPt );
	if( ret && ( t <= len ) ) {
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return 1;
	}
	return 0;
}

int LineSegmentvCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( CirclevLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return 1;
	}
	return 0;
}

int HalfSpacevLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;
	Vector2 dir;
	vec2_Subtract( &( fixed->lineSegment.posTwo ), &( fixed->lineSegment.posOne ), &dir );
	float len = vec2_Normalize( &dir );

	int ret = RayCastvHalfSpace( &( fixed->lineSegment.posOne ), &dir, primary, &t, &collPt );
	if( ret && ( t <= len ) ) {
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return 1;
	}
	return 0;
}

int LineSegmentvHalfSpace( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( HalfSpacevLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return 1;
	}
	return 0;
}

// if the line segments are collinear we find the first position along line 0 that hits line 1
bool collision_LineSegmentCollision( Vector2* l0p0, Vector2* l0p1, Vector2* l1p0, Vector2* l1p1, Vector2* outCollPos, float* outT )
{
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
//#error need to redo this, isn't finding every collision it should
	assert( l0p0 != NULL );
	assert( l0p1 != NULL );
	assert( l1p0 != NULL );
	assert( l1p1 != NULL );
	assert( outCollPos != NULL );
	assert( outT != NULL );
	if( FLT_EQ( vec2_DistSqrd( l0p0, l0p1 ), 0.0f ) ) return false;
	if( FLT_EQ( vec2_DistSqrd( l1p0, l1p1 ), 0.0f ) ) return false;

	Vector2 l0m;
	Vector2 l1m;
	Vector2 diffl01p0;

	// need them in parametric form
	vec2_Subtract( l0p1, l0p0, &l0m );
	vec2_Subtract( l1p1, l1p0, &l1m );

	vec2_Subtract( l1p0, l0p0, &diffl01p0 );

	// p = l0p0
	// q = l1p0
	// r = l0m
	// s = l1m
	// four cases:
	//  1. r x s == 0 and (q-p) x r = 0
	//     Lines are collinear, see if they overlap
	//  2. r x s == 0 and (q-p) x r != 0
	//     Lines are parallel and non-intersecting
	//  3. r x s != 0 and 0<=t<=1 and 0<=u<=1, lines meet at p+tr=q+us
	//  4. Otherwise the two line segments are not parallel but do not intersect

	float dirCross = vec2_CrossProduct( &l0m, &l1m );
	float diffDirCross = vec2_CrossProduct( &diffl01p0, &l0m );

	if( FLT_EQ( dirCross, 0.0f ) ) {
		if( FLT_EQ( diffDirCross, 0.0f ) ) {
			// collinear, see if they overlap
			float rDotr = vec2_DotProduct( &l0m, &l0m );
			assert( !FLT_EQ( rDotr, 0.0f ) );
			
			float t0 = vec2_DotProduct( &diffl01p0, &l0m ) / rDotr;
			float t1 = t0 + ( vec2_DotProduct( &l1m, &l0m ) / rDotr );

			// if [t0,t1] intersects [0,1] then there was a hit
			//  find the lowest of t0 and t1, then clamp to the range [0,1]
			float baseSign = sign( 0.0f - t0 );
			if( ( baseSign != sign( 1.0f - t0 ) ) ||
				( baseSign != sign( 0.0f - t1 ) ) ||
				( baseSign != sign( 1.0f - t1 ) ) ) {

				(*outT) = MIN( MIN( t0, t1 ), 0.0f );
				vec2_Lerp( l0p0, l0p1, (*outT), outCollPos );
				return true;
			}
		}
		// otherwise they are parallel and non-interesecting
	} else {
		float t = vec2_CrossProduct( &diffl01p0, &l1m ) / dirCross;
		float u = diffDirCross / dirCross;

		if( FLT_GE( t, 0.0f ) && FLT_LE( t, 1.0f ) && FLT_GE( u, 0.0f ) && FLT_LE( u, 1.0f ) ) {
			// got a collision point
			(*outT) = t;
			vec2_Lerp( l0p0, l0p1, t, outCollPos );
			return true;
		}
		// otherwise they are not parallel and do not intersect
	}

	return false; //*/

	/*float a1 = signed2DTriArea( l0p0, l0p1, l1p1 );
	float a2 = signed2DTriArea( l0p0, l0p1, l1p0 );

	// if the signs of a1 and a2 match there is no chance of collision
	//  except if they're collinear, which this doesn't detect
	if( a1 * a2 >= 0.0f ) {
		llog( LOG_DEBUG, "fail at a1 * a2" );
		return false;
	}

	float a3 = signed2DTriArea( l1p0, l1p1, l0p0 );
	float a4 = a3 + a2 - a1;
	if( a3 * a4 >= 0.0f ) {
		llog( LOG_DEBUG, "fail at a3 * a4" );
		return false;
	}

	float t = a3 / ( a3 - a4 );
	vec2_Lerp( l0p0, l0p1, t, outCollPos );
	(*outT) = t;

	return true;//*/
}

// this will not currently detect collinear collisions between line segments
int LineSegmentvLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;
	if( collision_LineSegmentCollision(
		&( primary->lineSegment.posOne ), &( primary->lineSegment.posTwo ),
		&( fixed->lineSegment.posOne ), &( fixed->lineSegment.posTwo ),
		&collPt, &t ) ) {

		if( t > 0.5f ) {
			vec2_Subtract( &collPt, &( primary->lineSegment.posTwo ), outSeparation );
		} else {
			vec2_Subtract( &collPt, &( primary->lineSegment.posOne ), outSeparation );
		}

		return 1;
	}

	return 0;
}

/* define all the collider functions, the first should always be the responding object, the second the fixed object */
typedef int(*CollisionCheck)( Collider* primary, Collider* fixed, Vector2* outSeparation );
CollisionCheck collisionChecks[NUM_COLLIDER_TYPES][NUM_COLLIDER_TYPES] = {
	{ AABBvAABB, AABBvCircle, AABBvHalfSpace, AABBvLineSegment },
	{ CirclevAABB, CirclevCircle, CirclevHalfSpace, CirclevLineSegment },
	{ HalfSpacevAABB, HalfSpacevCircle, invalidCollision, HalfSpacevLineSegment },
	{ LineSegmentvAABB, LineSegmentvCircle, LineSegmentvHalfSpace, LineSegmentvLineSegment }
};

/*
Finds the separation needed for c1 to move and not overlap c2.
 Returns 1 if there is any overlap and puts the separation into outSeparation.
 Returns 1 if there is no overlap.
*/
int collision_GetSeparation( Collider* c1, Collider* c2, Vector2* outSeparation )
{
	if( ( c1 == NULL ) || ( c2 == NULL ) || ( outSeparation == NULL ) ) {
		return 0;
	}

	if( ( c1->type == CT_DEACTIVATED ) || ( c2->type == CT_DEACTIVATED ) ) {
		return 0;
	}

	return collisionChecks[c1->type][c2->type]( c1, c2, outSeparation );
}

// just test to see if the mainCollider intersects any entries in the collection
bool collision_Test( Collider* mainCollider, ColliderCollection collection )
{
	if( ( mainCollider == NULL ) || ( mainCollider->type == CT_DEACTIVATED ) ) {
		return false;
	}

	Collider* current;
	char* data = (char*)collection.firstCollider;
	Vector2 ignore;
	for( int idx = 0; idx < collection.count; ++idx ) {
		current = (Collider*)( data + ( idx * collection.stride ) );

		if( current->type == CT_DEACTIVATED ) {
			continue;
		}

		if( collisionChecks[mainCollider->type][current->type]( mainCollider, current, &ignore ) ) {
			return true;
		}
	}

	return false;
}

/*
Finds and handles the all collisions between mainCollider and the colliders in the list.
 NOTE: You shouldn't modify the passed in list in the response.
*/
void collision_Detect( Collider* mainCollider, ColliderCollection collection, CollisionResponse response, int passThroughIdx )
{
	Vector2 separation = VEC2_ZERO;
	int idx;
	Collider* current;
	char* data = (char*)collection.firstCollider;

	if( ( mainCollider == NULL ) || ( mainCollider->type == CT_DEACTIVATED ) || ( data == NULL ) || ( response == NULL ) ) {
		return;
	}

	for( idx = 0; idx < collection.count; ++idx ) {
		current = (Collider*)( data + ( idx * collection.stride ) );

		if( current->type == CT_DEACTIVATED ) {
			continue;
		}

		if( collisionChecks[mainCollider->type][current->type]( mainCollider, current, &separation ) ) {
			response( passThroughIdx, idx, separation );
		}
	}
}

void collision_DetectAll( ColliderCollection firstCollection, ColliderCollection secondCollection, CollisionResponse response )
{
	Vector2 separation = VEC2_ZERO;
	Collider* firstCurrent;
	Collider* secondCurrent;
	char* firstData = (char*)firstCollection.firstCollider;
	char* secondData = (char*)secondCollection.firstCollider;

	/* if there's no response then there's no reason to detect any collisions */
	if( ( firstData == NULL ) || ( secondData == NULL ) || ( response == NULL ) ) {
		return;
	}

	for( int i = 0; i < firstCollection.count; ++i ) {
		firstCurrent = (Collider*)( firstData + ( i * firstCollection.stride ) );
		if( ( firstCurrent == NULL ) || ( firstCurrent->type == CT_DEACTIVATED ) ) {
			continue;
		}

		for( int j = 0; j < secondCollection.count; ++j ) {
			secondCurrent = (Collider*)( secondData + ( j * secondCollection.stride ) );
			if( ( secondCurrent == NULL ) || ( secondCurrent->type == CT_DEACTIVATED ) ) {
				continue;
			}

			if( collisionChecks[firstCurrent->type][secondCurrent->type]( firstCurrent, secondCurrent, &separation ) ) {
				response( i, j, separation );
			}
		}
	}
}

// detect all the collisions between objects in the collection
void collision_DetectAllInternal( ColliderCollection collection, CollisionResponse response )
{
	Vector2 separation = VEC2_ZERO;
	Collider* firstCurrent;
	Collider* secondCurrent;
	char* data = (char*)collection.firstCollider;

	/* if there's no response then there's no reason to detect any collisions */
	if( ( data == NULL ) || ( response == NULL ) ) {
		return;
	}

	for( int i = 0; i < collection.count; ++i ) {
		firstCurrent = (Collider*)( data + ( i * collection.stride ) );
		if( ( firstCurrent == NULL ) || ( firstCurrent->type == CT_DEACTIVATED ) ) {
			continue;
		}

		for( int j = i + 1; j < collection.count; ++j ) {
			secondCurrent = (Collider*)( data + ( j * collection.stride ) );
			if( ( secondCurrent == NULL ) || ( secondCurrent->type == CT_DEACTIVATED ) ) {
				continue;
			}

			if( collisionChecks[firstCurrent->type][secondCurrent->type]( firstCurrent, secondCurrent, &separation ) ) {
				response( i, j, separation );
			}
		}
	}
}

/*
Finds if the specified line segment hits anything in the list. Returns 1 if it did, 0 otherwise. Puts the
 collision point into out, if out is NULL it'll exit once it detects any collision instead of finding the first.
*/
int collision_RayCast( Vector2 start, Vector2 end, ColliderCollection collection, Vector2* out)
{
	int idx;
	Collider* current;
	Vector2 dir;
	char* data = (char*)collection.firstCollider;
	float t = 0.0f;
	Vector2 collPt = VEC2_ZERO;
	float currClosestT = 1.0f;
	int ret = 0;

	vec2_Subtract( &end, &start, &dir );

	for( idx = 0; idx < collection.count; ++idx ) {
		current = (Collider*)( data + ( idx * collection.stride ) );
		if( ( current == NULL ) || ( current->type == CT_DEACTIVATED ) ) {
			continue;
		}

		if( rayCastChecks[current->type]( &start, &dir, current, &t, &collPt ) ) {
			if( t <= currClosestT ) {
				ret = 1;
				currClosestT = t;
				if( out == NULL ) {
					return 1;
				}
				(*out) = collPt;
			}
		}
	}

	return ret;
}

/*
Finds the closest distance from the position to the collider, useful when you want the distance to object but not it's center.
 Returns a negative number if there were any problems.
 TODO: Find a way to remove the square root from the circle distance check
*/
float collision_Distance( Collider* collider, Vector2* pos )
{
	float dist = -1.0f;
	
	switch( collider->type ) {
	case CT_CIRCLE: {
		Vector2 diff;
		vec2_Subtract( pos, &( collider->circle.center ), &diff );
		dist = vec2_Mag( &diff ) - collider->circle.radius;
		if( dist < 0.0f ) {
			// inside the circle
			dist = 0.0f;
		}
	} break;
	case CT_AABB: {
		dist = 0.0f;
		Vector2 min;
		Vector2 max;
		vec2_Add( &( collider->aabb.center ), &( collider->aabb.halfDim ), &max );
		vec2_Subtract( &( collider->aabb.center ), &( collider->aabb.halfDim ), &min );
		
		// calculates squared distance
		if( pos->v[0] < min.v[0] ) { dist += ( ( min.v[0] - pos->v[0] ) * ( min.v[0] - pos->v[0] ) ); }
		if( pos->v[0] > max.v[0] ) { dist += ( ( pos->v[0] - max.v[0] ) * ( pos->v[0] - max.v[0] ) ); }
		if( pos->v[1] < min.v[1] ) { dist += ( ( min.v[1] - pos->v[1] ) * ( min.v[1] - pos->v[1] ) ); }
		if( pos->v[1] > max.v[1] ) { dist += ( ( pos->v[1] - max.v[1] ) * ( pos->v[1] - max.v[1] ) ); }

		dist = sqrtf( dist );
	} break;
	case CT_DEACTIVATED: {
		dist = -1.0f;
	} break;
	default:
		llog( LOG_WARN, "Attempting to find distance from collider what has no distance function." );
		break;
	}

	return dist;
}

/*
Helper function to create a half-space collider from a position and normal.
*/
void collision_CalculateHalfSpace( const Vector2* pos, const Vector2* normal, Collider* outCollider )
{
	assert( outCollider != NULL );
	assert( pos != NULL );
	assert( normal != NULL );

	outCollider->type = CT_HALF_SPACE;
	outCollider->halfSpace.normal = (*normal);
	vec2_Normalize( &( outCollider->halfSpace.normal ) );
	outCollider->halfSpace.d = vec2_DotProduct( &( outCollider->halfSpace.normal ), pos );
}

/*
Renders collision shapes.
*/
void collision_DebugDrawing( Collider* collision, unsigned int camFlags, Color color )
{
	Vector2 size;
	Vector2 position;

	switch( collision->type ) {
	case CT_AABB:
		vec2_Scale( &( collision->aabb.halfDim ), 2.0f, &size );
		vec2_Subtract( &( collision->aabb.center ), &( collision->aabb.halfDim ), &position );
		debugRenderer_AABB( camFlags, position, size, color );
		break;
	case CT_CIRCLE:
		debugRenderer_Circle( camFlags, collision->circle.center, collision->circle.radius, color );
		break;
	case CT_LINE_SEGMENT:
		debugRenderer_Line( camFlags, collision->lineSegment.posOne, collision->lineSegment.posTwo, color );
		break;
	}
}

void collision_CollectionDebugDrawing( ColliderCollection collection, unsigned int camFlags, Color color )
{
	Collider* current;
	char* data = (char*)collection.firstCollider;

	for( int i = 0; i < collection.count; ++i ) {
		current = (Collider*)( data + ( i * collection.stride ) );
		collision_DebugDrawing( current, camFlags, color );
	}
}

bool collision_IsPointInsideComplexPolygon( Vector2* pos, Vector2* polygon, size_t numPts )
{
	assert( pos != NULL );
	assert( polygon != NULL );
	assert( numPts >= 3 );

	// http://alienryderflex.com/polygon/

	// we have to trace a horizontal ray starting at pos, if it hits an odd number
	//  of segments of the polygon then it's inside
	// we'll assume the ray goes along the positive x-axis

	size_t j = numPts - 1;
	bool oddNodes = false;

	for( size_t i = 0; i < numPts; ++i ) {
		if( ( ( ( polygon[i].y < pos->y ) && ( polygon[j].y >= pos->y ) ) ||
			  ( ( polygon[j].y < pos->y ) && ( polygon[i].y >= pos->y ) ) ) &&
			( ( polygon[i].x <= pos->x ) || ( polygon[j].x <= pos->x ) ) ) {

			// find where along the segment it would collide
			float t = ( pos->y - polygon[i].y ) / ( polygon[j].y - polygon[i].y );
			// get the x coord of the position
			float x = polygon[i].x + ( t * ( polygon[j].x - polygon[i].x ) );
			// see if we hit
			oddNodes ^= ( x < pos->x );
		}

		j = i;
	}

	return oddNodes;
}