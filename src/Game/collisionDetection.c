#include "collisionDetection.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>

#include "Graphics/debugRendering.h"
#include "Math/vector2.h"
#include "Math/matrix3.h"
#include "Math/mathUtil.h"
#include "Utils/stretchyBuffer.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"

// raycasting intersection functions
static bool invalidRayCastType( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	llog( LOG_WARN, "Attempting to detect a collision between a ray and a collider that has no valid detection method." );
	return false;
}

static bool RayCastvAABBAxis( float rayStart, float dir, float boxMin, float boxMax, float* tMin, float* tMax )
{
	float ood;
	float t1, t2;
	float swap;

	if( fabsf( dir ) < 0.001f ) {
		// ray parallel to axis, no hit unless origin within slab
		if( ( rayStart < boxMin ) || ( rayStart > boxMax ) ) {
			return false;
		}
	} else {
		// compute intersection of the ray with the near and far line of the slab
		ood = 1.0f / dir;
		t1 = ( boxMin - rayStart ) * ood;
		t2 = ( boxMax - rayStart ) * ood;

		// make t1 the intersection with the near plane, t2 with the far plane
		if( t1 > t2 ) {
			swap = t1;
			t1 = t2;
			t2 = swap;
		}

		// compute intersection
		(*tMin) = MAX( (*tMin), t1 );
		(*tMax) = MIN( (*tMax), t2 );

		if( (*tMin) > (*tMax) ) {
			return false;
		}
	}

	return true;
}

static bool LineAABBTest( Vector2* start, Vector2* end, ColliderAAB* aabb, float maxAllowedT, float minAllowedT, float* t, Vector2* pt )
{
	float tMin, tMax;

	tMin = minAllowedT;
	tMax = maxAllowedT;

	if( !RayCastvAABBAxis( start->v[0], ( end->v[0] - start->v[0] ),
							( aabb->center.v[0] - aabb->halfDim.v[0] ), ( aabb->center.v[0] + aabb->halfDim.v[0] ),
							&tMin, &tMax ) ) {
		return false;
	}

	if( !RayCastvAABBAxis( start->v[1], ( end->v[1] - start->v[1] ),
							( aabb->center.v[1] - aabb->halfDim.v[1] ), ( aabb->center.v[1] + aabb->halfDim.v[1] ),
							&tMin, &tMax ) ) {
		return false;
	}

	vec2_AddScaled( start, end, tMin, pt );
	(*t) = tMin;

	return true;
}

static bool RayCastvAABB( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	return LineAABBTest( start, dir, &(collider->aabb), FLT_MAX, 0.0f, t, pt );
}

static bool RayCastvCircle( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	Vector2 startCircOrig;
	float a, b, c, discr;


	// translate the sphere to the origin
	vec2_Subtract( start, &( collider->circle.center ), &startCircOrig );
	b = 2.0f * 	vec2_DotProduct( &startCircOrig, dir );
	c = vec2_DotProduct( &startCircOrig, &startCircOrig ) - ( collider->circle.radius * collider->circle.radius );

	// rays origin is outside the circle, and it's pointing away, so no chance of hitting
	if( ( c > 0.0f ) && ( b > 0.0f ) ) {
		return false;
	}

	a = vec2_DotProduct( dir, dir );
	discr = ( b * b ) - ( 4 * a * c );

	// ray missed the circle
	if( discr < 0.0f ) {
		return false;
	}

	(*t) = ( -b - sqrtf( discr ) ) / ( 2 * a );

	// if the ray starts inside the sphere
	if( (*t) < 0.0f ) {
		(*t) = 0.0f;
	}

	vec2_AddScaled( start, dir, (*t), pt );

	return true;
}

static bool RayCastvHalfSpace( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	// if the ray starts inside the half space we'll consider the collision at the rays origin
	float nDotA = vec2_DotProduct( &( collider->halfSpace.normal ), start );
	float nDotDir = vec2_DotProduct( &( collider->halfSpace.normal ), dir );
	(*t) = ( collider->halfSpace.d - nDotA ) / nDotDir;

	if( (*t) >= 0.0f ) {
		vec2_AddScaled( start, dir, (*t), pt );
		return true;
	}

	return false;
}

static bool RayCastvBox( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt )
{
	*pt = VEC2_ZERO;
	Matrix3 mat = IDENTITY_MATRIX_3;
	Matrix3 inverseMat;

	mat3_SetColumn2( &mat, 0, &collider->box.axes[1] );
	mat3_SetColumn2( &mat, 1, &collider->box.axes[2] );
	mat3_SetColumn2( &mat, 2, &collider->box.center );

	mat3_Inverse( &mat, &inverseMat );

	Vector2 localStart, localDir;

	mat3_TransformVec2Pos( &inverseMat, start, &localStart );
	mat3_TransformVec2Dir( &inverseMat, dir, &localDir );

	Collider aabb;
	aabb.type = CT_AAB;
	aabb.aabb.center = VEC2_ZERO;
	aabb.aabb.halfDim = collider->aabb.halfDim;

	bool collided = RayCastvAABB( &localStart, &localDir, &aabb, t, pt );

	// collision point is local to rectangle, so translate it back
	mat3_TransformVec2Pos_InPlace( &mat, pt );

	return collided;
}

typedef bool(*RayCastCheck)( Vector2* start, Vector2* dir, Collider* collider, float* t, Vector2* pt );
RayCastCheck rayCastChecks[NUM_COLLIDER_TYPES] = { RayCastvAABB, RayCastvCircle, RayCastvHalfSpace, invalidRayCastType, RayCastvBox };

// Standard Collision Functions
//  For all of these we pass in a primary as the first, and a fixed as the second. The outSeparation is how far the primary
//  has to move to no longer be colliding with the fixed.

//  used if we don't want to ever handle collisions between these two types.
static bool invalidCollision( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	llog( LOG_WARN, "Attempting to detect a collision between two collider types that has no valid detection method." );
	return false;
}

static bool AABBvHalfSpace( Collider* primary, Collider* fixed, Vector2* outSeparation )
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
		float dist = vec2_DotProduct( &( corners[i] ), &( fixed->halfSpace.normal ) ) - fixed->halfSpace.d;
		if( dist < deepest ) {
			deepest = dist;
		}
	}

	if( deepest >= 0.0f ) {
		return false;
	}

	vec2_Scale( &( fixed->halfSpace.normal ), -deepest, outSeparation );
	return true;
}

static bool HalfSpacevAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( AABBvHalfSpace( primary, fixed, outSeparation ) ) {
		outSeparation->x = -outSeparation->x;
		outSeparation->y = -outSeparation->y;
		return true;
	}

	return false;
}

static bool CirclevHalfSpace( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	float dist = vec2_DotProduct( &( primary->circle.center ), &( fixed->halfSpace.normal ) ) - fixed->halfSpace.d;
	if( dist > primary->circle.radius ) {
		return false;
	}

	vec2_Scale( &( fixed->halfSpace.normal ), ( primary->circle.radius - dist ), outSeparation );

	return true;
}

static bool HalfSpacevCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( CirclevHalfSpace( primary, fixed, outSeparation ) ) {
		outSeparation->x = -outSeparation->x;
		outSeparation->y = -outSeparation->y;
		return true;
	}

	return false;
}

static bool AABBvAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
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
		return false;
	}

	// collision on both axi, figure out how much we have to move to stop colliding
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

	return true;
}

static bool CirclevAABB( Collider* primary, Collider* fixed, Vector2* outSeparation  )
{
	float xPenetration;
	float yPenetration;
	Vector2 diff;
	Vector2 corner;
	float sumDim;
	float diffLen;
	float separationLen;

	vec2_Subtract( &( primary->aabb.center ), & ( fixed->aabb.center ), &diff );

	// first check to make sure collision is possible
	sumDim = primary->circle.radius + fixed->aabb.halfDim.v[0];
	xPenetration = sumDim - fabsf( diff.v[0] );
	if( xPenetration <= 0 ) {
		return false;
	}

	sumDim = primary->circle.radius + fixed->aabb.halfDim.v[1];
	yPenetration = sumDim - fabsf( diff.v[1] );
	if( yPenetration <= 0 ) {
		return false;
	}

	// if the circle is in one of the corner zones for the box then check against that corner that's all we should need to check
	if( ( fabsf( diff.v[0] ) >= fixed->aabb.halfDim.v[0] ) && ( fabsf( diff.v[1] ) >= fixed->aabb.halfDim.v[1] ) ) {
		// first figure out which corner the circle is closest to
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

		// check to see if the distance from the corner is smaller enough for a collision
		if( vec2_MagSqrd( &diff ) >= ( primary->circle.radius * primary->circle.radius ) ) {
			return false;
		}

		// now calculate the actual separation amount needed
		diffLen = vec2_Mag( &diff );
		separationLen = primary->circle.radius - diffLen;
		outSeparation->v[0] = ( diff.v[0] / diffLen ) * separationLen;
		outSeparation->v[1] = ( diff.v[1] / diffLen ) * separationLen;
	} else {
		// otherwise just check like a normal the circle was an aabb with half lengths equal to it's radius
		// collision on both axi, figure out how much we have to move to stop colliding
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

	return true;
}

static bool AABBvCircle( Collider* primary, Collider* fixed, Vector2* outSeparation  )
{
	// use existing code and just flip the separation
	if( CirclevAABB( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}

	return false;
}

static bool CirclevCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	float radiiSum;
	float dist;

	radiiSum = ( primary->circle.radius + fixed->circle.radius );
	radiiSum *= radiiSum;
	vec2_Subtract( &( primary->circle.center ), &( fixed->circle.center ), outSeparation );
	dist = vec2_MagSqrd( outSeparation );

	if( dist >= radiiSum ) {
		return false;
	}

	// lots of square roots, better way to do this?
	radiiSum = sqrtf( radiiSum );
	dist = sqrtf( dist );
	vec2_Normalize( outSeparation );
	vec2_Scale( outSeparation, ( radiiSum - dist ), outSeparation );

	return true;
}

static bool AABBvLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;

	if( LineAABBTest( &( fixed->lineSegment.posOne ), &( fixed->lineSegment.posTwo ), &(primary->aabb), 1.0f, 0.0f, &t, &collPt ) ) {
		// the line segment is fixed
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return true;
	}

	return false;
}

static bool LineSegmentvAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( AABBvLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

static bool CirclevLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;
	Vector2 dir;
	vec2_Subtract( &( fixed->lineSegment.posTwo ), &( fixed->lineSegment.posOne ), &dir );
	float len = vec2_Normalize( &dir );

	int ret = RayCastvCircle( &( fixed->lineSegment.posOne ), &dir, primary, &t, &collPt );
	if( ret && ( t <= len ) ) {
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return true;
	}
	return false;
}

static bool LineSegmentvCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( CirclevLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

static bool HalfSpacevLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	Vector2 collPt;
	float t;
	Vector2 dir;
	vec2_Subtract( &( fixed->lineSegment.posTwo ), &( fixed->lineSegment.posOne ), &dir );
	float len = vec2_Normalize( &dir );

	int ret = RayCastvHalfSpace( &( fixed->lineSegment.posOne ), &dir, primary, &t, &collPt );
	if( ret && ( t <= len ) ) {
		vec2_Subtract( &( fixed->lineSegment.posOne ), &collPt, outSeparation );
		return true;
	}
	return false;
}

static bool LineSegmentvHalfSpace( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( HalfSpacevLineSegment( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

// if the line segments are collinear we find the first position along line 0 that hits line 1
bool collision_LineSegmentCollision( Vector2* l0p0, Vector2* l0p1, Vector2* l1p0, Vector2* l1p1, Vector2* outCollPos, float* outT )
{
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
	ASSERT_AND_IF_NOT( l0p0 != NULL ) return false;
	ASSERT_AND_IF_NOT( l0p1 != NULL ) return false;
	ASSERT_AND_IF_NOT( l1p0 != NULL ) return false;
	ASSERT_AND_IF_NOT( l1p1 != NULL ) return false;
	ASSERT_AND_IF_NOT( outCollPos != NULL ) return false;
	ASSERT_AND_IF_NOT( outT != NULL ) return false;
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
			SDL_assert( !FLT_EQ( rDotr, 0.0f ) );
			
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

	return false;
}

// this will not currently detect collinear collisions between line segments
static bool LineSegmentvLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
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

		return true;
	}

	return false;
}

static bool BoxvBox( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	// since we're only doing 2d then we can just use SAT
	ColliderBox* colliders[2] = { &primary->box, &fixed->box };

	float smallestSeparation = FLT_MAX;

	for( int i = 0; i < ARRAY_SIZE( colliders ); ++i ) {
		ColliderBox* testing = colliders[i];
		ColliderBox* against = colliders[1 - i];

		Vector2 relativePoints[4];
		Vector2 testingRelativePos;

		vec2_Subtract( &testing->center, &against->center, &testingRelativePos );

		vec2_AddScaled( &testingRelativePos, &testing->axes[0], testing->halfDim.h, &relativePoints[0] );
		vec2_AddScaled( &relativePoints[0],	&testing->axes[1], testing->halfDim.w, &relativePoints[0] );

		vec2_AddScaled( &testingRelativePos, &testing->axes[0], testing->halfDim.h, &relativePoints[1] );
		vec2_AddScaled( &relativePoints[1], &testing->axes[1], -testing->halfDim.w, &relativePoints[1] );

		vec2_AddScaled( &testingRelativePos, &testing->axes[0], -testing->halfDim.h, &relativePoints[2] );
		vec2_AddScaled( &relativePoints[2], &testing->axes[1], -testing->halfDim.w, &relativePoints[2] );

		vec2_AddScaled( &testingRelativePos, &testing->axes[0], -testing->halfDim.h, &relativePoints[3] );
		vec2_AddScaled( &relativePoints[3], &testing->axes[1], testing->halfDim.w, &relativePoints[3] );
		
		for( int a = 0; a < ARRAY_SIZE( against->axes ); ++a ) {
			float minDist = FLT_MAX;
			float maxDist = -FLT_MAX;

			for( int b = 0; b < 4; ++b ) {
				float dist = vec2_DotProduct( &relativePoints[b], &against->axes[a] );
				minDist = MIN( minDist, dist );
				maxDist = MAX( maxDist, dist );
			}

			// see if there's a separating axis, if there is we can early out as there is no collision
			float againstMin = -against->halfDim.v[1 - a];
			float againstMax = against->halfDim.v[1 - a];

			if( !( ( ( minDist < againstMax ) && ( minDist > againstMin ) ) || ( ( againstMin < maxDist ) && ( againstMin > minDist ) ) ) ) {
				return false;
			}

			// get the smallest amount it would have to be separated to no longer collide
			float diffs[2] = { againstMin - maxDist, againstMax - minDist };
			float separation = SDL_fabsf( diffs[0] ) < SDL_fabsf( diffs[1] ) ? diffs[0] : diffs[1];
			if( SDL_fabsf( separation ) < SDL_fabsf( smallestSeparation ) ) {
				smallestSeparation = separation;
				if( i == 1 ) {
					// testing is fixed collider, against is primary, calculations are how much testing needs to move but want primary to move instead of fixed
					separation = -separation;
				}
				vec2_Scale( &against->axes[a], separation, outSeparation );
			}
		}
	}

	return true;
}

static bool AABBvBox( Collider* aabbPrimary, Collider* boxFixed, Vector2* outSeparation )
{
	// just treat the AABB as a box with a rotation as 0 and do the box v box collision
	Collider boxAsAABB;
	boxAsAABB.type = CT_ORIENTED_BOX;
	boxAsAABB.box.center = aabbPrimary->aabb.center;
	boxAsAABB.box.halfDim = aabbPrimary->aabb.halfDim;
	boxAsAABB.box.axes[0] = VEC2_UP;
	boxAsAABB.box.axes[1] = VEC2_RIGHT;

	return BoxvBox( &boxAsAABB, boxFixed, outSeparation );
}

static bool CirclevBox( Collider* circlePrimary, Collider* boxFixed, Vector2* outSeparation )
{
	// find point on box closest to circle
	Vector2 d;
	vec2_Subtract( &circlePrimary->circle.center, &boxFixed->box.center, &d );

	Vector2 closestPointOnBox;
	closestPointOnBox = boxFixed->box.center;

	for( int i = 0; i < ARRAY_SIZE( boxFixed->box.axes ); ++i ) {
		// project d onto the axis to get the distance
		float dist = vec2_DotProduct( &d, &boxFixed->box.axes[i] );

		// clamp to box if outside extents
		// 0 is width, 1 is height, this is opposite the axes where 0 is the up normal and 1 is the right
		int di = 1 - i;
		dist = clamp( -boxFixed->box.halfDim.v[di], boxFixed->box.halfDim.v[di], dist );

		// step that distance along that axis to get the world coordinates
		vec2_AddScaled( &closestPointOnBox, &boxFixed->box.axes[i], dist, &closestPointOnBox );
	}

	// we now have the closest point, see if the distance from there to the circle is less than the radius of the circle
	Vector2 diff;
	vec2_Subtract( &closestPointOnBox, &circlePrimary->circle.center, &diff );
	float dist = vec2_MagSqrd( &diff );
	if( dist > ( circlePrimary->circle.radius * circlePrimary->circle.radius ) ) {
		return false;
	}

	// now we need to figure out the separation vector, normalize diff, then find difference between the closest point on the box and project of that diff onto the circle
	dist = SDL_sqrtf( dist );
	diff.x /= dist;
	diff.y /= dist;

	Vector2 circlePoint;
	vec2_AddScaled( &circlePrimary->circle.center, &diff, circlePrimary->circle.radius, &circlePoint );

	// box is fixed so we want it pointing towards where the circle would have to move
	vec2_Subtract( &circlePoint, &closestPointOnBox, outSeparation );

	return true;
}

static bool HalfSpacevBox( Collider* halfSpacePrimary, Collider* boxFixed, Vector2* outSeparation )
{
	// projects the box onto the plane normal, giving the half-length of the box on it
	float radius =
		boxFixed->box.halfDim.h * SDL_fabsf( vec2_DotProduct( &halfSpacePrimary->halfSpace.normal, &boxFixed->box.axes[0] ) ) +
		boxFixed->box.halfDim.w * SDL_fabsf( vec2_DotProduct( &halfSpacePrimary->halfSpace.normal, &boxFixed->box.axes[1] ) );

	// distance of box center from plane
	float signedDistance = vec2_DotProduct( &halfSpacePrimary->halfSpace.normal, &boxFixed->box.center ) - halfSpacePrimary->halfSpace.d;

	vec2_Scale( &halfSpacePrimary->halfSpace.normal, -( signedDistance - radius ), outSeparation );

	return signedDistance <= radius;
}

static bool LineSegmentvBox( Collider* lineSegmentPrimary, Collider* boxFixed, Vector2* outSeparation )
{
	// project the line onto the box axes
	Vector2 relativePoints[2];
	float smallestSeparation = FLT_MAX;

	vec2_Subtract( &lineSegmentPrimary->lineSegment.posOne, &boxFixed->box.center, &relativePoints[0] );
	vec2_Subtract( &lineSegmentPrimary->lineSegment.posTwo, &boxFixed->box.center, &relativePoints[1] );

	for( int a = 0; a < ARRAY_SIZE( boxFixed->box.axes ); ++a ) {
		float minDist = FLT_MAX;
		float maxDist = -FLT_MAX;

		for( int b = 0; b < 2; ++b ) {
			float dist = vec2_DotProduct( &relativePoints[b], &boxFixed->box.axes[a] );
			minDist = MIN( minDist, dist );
			maxDist = MAX( maxDist, dist );
		}

		// see if there's a separating axis, if there is we can early out as there is no collision
		float againstMin = -boxFixed->box.halfDim.v[1 - a];
		float againstMax = boxFixed->box.halfDim.v[1 - a];

		if( !( ( ( minDist < againstMax ) && ( minDist > againstMin ) ) || ( ( againstMin < maxDist ) && ( againstMin > minDist ) ) ) ) {
			return false;
		}

		// get the smallest amount it would have to be separated to no longer collide
		float diffs[2] = { againstMin - maxDist, againstMax - minDist };
		float separation = SDL_fabsf( diffs[0] ) < SDL_fabsf( diffs[1] ) ? diffs[0] : diffs[1];
		if( SDL_fabsf( separation ) < SDL_fabsf( smallestSeparation ) ) {
			smallestSeparation = separation;
			vec2_Scale( &boxFixed->box.axes[a], separation, outSeparation );
		}
	}

	return true;
}

static bool BoxvAABB( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( AABBvBox( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

static bool BoxvCircle( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( CirclevBox( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

static bool BoxvHalfSpace( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( HalfSpacevBox( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

static bool BoxvLineSegment( Collider* primary, Collider* fixed, Vector2* outSeparation )
{
	if( LineSegmentvBox( fixed, primary, outSeparation ) ) {
		outSeparation->v[0] = -( outSeparation->v[0] );
		outSeparation->v[1] = -( outSeparation->v[1] );
		return true;
	}
	return false;
}

// define all the collider functions, the first should always be the responding object, the second the fixed object
typedef bool(*CollisionCheck)( Collider* primary, Collider* fixed, Vector2* outSeparation );
CollisionCheck collisionChecks[NUM_COLLIDER_TYPES][NUM_COLLIDER_TYPES] = {
	{ AABBvAABB, AABBvCircle, AABBvHalfSpace, AABBvLineSegment, AABBvBox },
	{ CirclevAABB, CirclevCircle, CirclevHalfSpace, CirclevLineSegment, CirclevBox },
	{ HalfSpacevAABB, HalfSpacevCircle, invalidCollision, HalfSpacevLineSegment, HalfSpacevBox },
	{ LineSegmentvAABB, LineSegmentvCircle, LineSegmentvHalfSpace, LineSegmentvLineSegment, LineSegmentvBox },
	{ BoxvAABB, BoxvCircle, BoxvHalfSpace, BoxvLineSegment, BoxvBox }
};

// Finds the separation needed for c1 to move and not overlap c2.
//  Returns 1 if there is any overlap and puts the separation into outSeparation.
//  Returns 1 if there is no overlap.
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

// Finds and handles the all collisions between mainCollider and the colliders in the list.
//  NOTE: You shouldn't modify the passed in list in the response.
void collision_Detect( Collider* mainCollider, ColliderCollection collection, CollisionResponse response, size_t passThroughIdx )
{
	Vector2 separation = VEC2_ZERO;
	size_t idx;
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

	// if there's no response then there's no reason to detect any collisions
	if( ( firstData == NULL ) || ( secondData == NULL ) || ( response == NULL ) ) {
		return;
	}

	for( size_t i = 0; i < firstCollection.count; ++i ) {
		firstCurrent = (Collider*)( firstData + ( i * firstCollection.stride ) );
		if( ( firstCurrent == NULL ) || ( firstCurrent->type == CT_DEACTIVATED ) ) {
			continue;
		}

		for( size_t j = 0; j < secondCollection.count; ++j ) {
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

	// if there's no response then there's no reason to detect any collisions
	if( ( data == NULL ) || ( response == NULL ) ) {
		return;
	}

	for( size_t i = 0; i < collection.count; ++i ) {
		firstCurrent = (Collider*)( data + ( i * collection.stride ) );
		if( ( firstCurrent == NULL ) || ( firstCurrent->type == CT_DEACTIVATED ) ) {
			continue;
		}

		for( size_t j = i + 1; j < collection.count; ++j ) {
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

// Finds if the specified line segment hits anything in the list. Returns 1 if it did, 0 otherwise. Puts the
//  collision point into out, if out is NULL it'll exit once it detects any collision instead of finding the first.
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

// Finds the closest distance from the position to the collider, useful when you want the distance to object but not it's center.
//  Returns a negative number if there were any problems.
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
	case CT_AAB: {
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
		// TODO: Add line segment, half-space, and oriented box
		llog( LOG_WARN, "Attempting to find distance from collider what has no distance function." );
		break;
	}

	return dist;
}

//Helper function to create a half-space collider from a position and normal.
void collision_CalculateHalfSpace( const Vector2* pos, const Vector2* normal, Collider* outCollider )
{
	ASSERT_AND_IF_NOT( outCollider != NULL ) return;
	outCollider->type = CT_HALF_SPACE;
	// default values for out collider if it's provided
	outCollider->halfSpace.pos = VEC2_ZERO;
	outCollider->halfSpace.d = 0.0f;
	outCollider->halfSpace.normal = VEC2_UP;

	ASSERT_AND_IF_NOT( pos != NULL ) return;
	ASSERT_AND_IF_NOT( normal != NULL ) return;

	outCollider->halfSpace.normal = *normal;
	vec2_Normalize( &( outCollider->halfSpace.normal ) );
	outCollider->halfSpace.d = vec2_DotProduct( &( outCollider->halfSpace.normal ), pos );
	outCollider->halfSpace.pos = *pos;
}

// Helper functions for oriented boxes.
void collision_CalculateOrientedBox( const Vector2* pos, float rotRad, const Vector2* halfSize, Collider* outCollider )
{
	ASSERT_AND_IF_NOT( outCollider != NULL ) return;

	// default values if out collider is provided
	outCollider->type = CT_ORIENTED_BOX;
	outCollider->box.center = VEC2_ZERO;
	outCollider->box.halfDim = VEC2_ZERO;
	outCollider->box.axes[0] = VEC2_UP;
	outCollider->box.axes[1] = VEC2_RIGHT;

	ASSERT_AND_IF_NOT( pos != NULL ) return;
	ASSERT_AND_IF_NOT( halfSize != NULL ) return;

	outCollider->box.center = *pos;
	outCollider->box.halfDim = *halfSize;
	vec2_NormalFromRot( rotRad, &outCollider->box.axes[0] );
	vec2_PerpRight( &outCollider->box.axes[0], &outCollider->box.axes[1] );
}

float collision_GetOrientedBoxRadianRotation( const Collider* boxCollider )
{
	ASSERT_AND_IF_NOT( boxCollider != NULL ) return 0.0f;
	ASSERT_AND_IF_NOT( boxCollider->type == CT_ORIENTED_BOX ) return 0.0f;

	return vec2_RotationRadians( &boxCollider->box.axes[0] );
}

// Renders collision shapes.
void collision_DebugDrawing( Collider* collision, unsigned int camFlags, Color color )
{
	Vector2 size;
	Vector2 position;

	switch( collision->type ) {
	case CT_AAB:
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
	case CT_HALF_SPACE:
		{
			Vector2 normalPerp = vec2( collision->halfSpace.normal.y, -collision->halfSpace.normal.x );

			Vector2 trianglePoints[3];
			vec2_AddScaled( &collision->halfSpace.pos, &collision->halfSpace.normal, 8.0f, &trianglePoints[0] );
			vec2_AddScaled( &collision->halfSpace.pos, &normalPerp, 8.0f, &trianglePoints[1] );
			vec2_AddScaled( &collision->halfSpace.pos, &normalPerp, -8.0f, &trianglePoints[2] );
			debugRenderer_Triangle( camFlags, trianglePoints[0], trianglePoints[1], trianglePoints[2], color );

			Vector2 linePoints[2];
			vec2_AddScaled( &collision->halfSpace.pos, &normalPerp, 10000.0f, &linePoints[0] );
			vec2_AddScaled( &collision->halfSpace.pos, &normalPerp, -10000.0f, &linePoints[1] );
			debugRenderer_Line( camFlags, linePoints[0], linePoints[1], color );
		}
		break;
	case CT_ORIENTED_BOX:
		{
			Vector2 points[4];

			vec2_AddScaled( &collision->box.center, &collision->box.axes[0], collision->box.halfDim.h, &points[0] );
			vec2_AddScaled( &points[0], &collision->box.axes[1], collision->box.halfDim.w, &points[0] );

			vec2_AddScaled( &collision->box.center, &collision->box.axes[0], collision->box.halfDim.h, &points[1] );
			vec2_AddScaled( &points[1], &collision->box.axes[1], -collision->box.halfDim.w, &points[1] );

			vec2_AddScaled( &collision->box.center, &collision->box.axes[0], -collision->box.halfDim.h, &points[2] );
			vec2_AddScaled( &points[2], &collision->box.axes[1], -collision->box.halfDim.w, &points[2] );

			vec2_AddScaled( &collision->box.center, &collision->box.axes[0], -collision->box.halfDim.h, &points[3] );
			vec2_AddScaled( &points[3], &collision->box.axes[1], collision->box.halfDim.w, &points[3] );

			for( int i = 0; i < 4; ++i ) {
				debugRenderer_Line( camFlags, points[i], points[(i + 1) % 4], color );
			}
		}
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
	ASSERT_AND_IF_NOT( pos != NULL ) return false;
	ASSERT_AND_IF_NOT( polygon != NULL ) return false;
	ASSERT_AND_IF_NOT( numPts >= 3 ) return false;

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