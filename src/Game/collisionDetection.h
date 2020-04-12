#ifndef JTR_COLLISION_DETECTION_H
#define JTR_COLLISION_DETECTION_H
/*
Detects collisions and calls functions to resolve them.
Based on this:
http://www.metanetsoftware.com/technique/tutorialA.html
*/

// TODO: Rename and adjust everything

#include <stddef.h>
#include <stdbool.h>
#include "Math/vector2.h"
#include "Graphics/color.h"

enum ColliderType {
	CT_DEACTIVATED = -1,
	CT_AABB,
	CT_CIRCLE,
	CT_HALF_SPACE,
	CT_LINE_SEGMENT,
	NUM_COLLIDER_TYPES
};

typedef struct {
	enum ColliderType type;
	Vector2 center;
	Vector2 halfDim;
} ColliderAABB;

typedef struct {
	enum ColliderType type;
	Vector2 center;
	float radius;
} ColliderCircle;

typedef struct {
	enum ColliderType type;
	Vector2 normal;
	float d;
} ColliderHalfSpace;

typedef struct {
	enum ColliderType type;
	Vector2 posOne;
	Vector2 posTwo;
} ColliderLineSegment;

typedef union {
	enum ColliderType type;
	ColliderAABB aabb;
	ColliderCircle circle;
	ColliderHalfSpace halfSpace;
	ColliderLineSegment lineSegment;
} Collider;

typedef struct {
	Collider* firstCollider;
	size_t stride;
	int count;
} ColliderCollection;

typedef void(*CollisionResponse)( int firstColliderIdx, int secondColliderIdx, Vector2 separation );

/*
Finds the separation needed for c1 to move and not overlap c2.
 Returns 1 if there is any overlap and puts the separation into outSeparation.
 Returns 1 if there is no overlap.
*/
int collision_GetSeparation( Collider* c1, Collider* c2, Vector2* outSeparation );

// just test to see if the mainCollider intersects any entries in the collection
bool collision_Test( Collider* mainCollider, ColliderCollection collection );

/*
Finds and handles the all collisions between mainCollider and the colliders in the list.
 NOTE: You shouldn't modify the passed in colliders in the response.
*/
void collision_Detect( Collider* mainCollider, ColliderCollection collection, CollisionResponse response, int passThroughIdx );

/*
Finds all the collisions between every collider in firstCollectin and secondCollection.
 The indices passed to the response function match the collection, firstColliderIdx is the index in firstCollection
 and secondColliderIdx is the index in the secondCollection.
*/
void collision_DetectAll( ColliderCollection firstCollection, ColliderCollection secondCollection, CollisionResponse response );

/*
Finds all the collision between every collider inside the collection.
 The indices passed to the response function match the collection.
*/
void collision_DetectAllInternal( ColliderCollection collection, CollisionResponse response );

/*
Finds if the specified line segment hits anything in the list. Returns 1 if it did, 0 otherwise. Puts the
 collision point into out, if out is NULL it'll exit once it detects any collision instead of finding the first.
*/
int collision_RayCast( Vector2 start, Vector2 end, ColliderCollection collection, Vector2* out );

/*
Finds the closest distance from the position to the collider, useful when you want the distance to object but not it's center.
 Returns a negative number if there were any problems.
*/
float collision_Distance( Collider* collider, Vector2* position );

/*
Helper function to create a half-space collider from a position and normal.
*/
void collision_CalculateHalfSpace( const Vector2* pos, const Vector2* normal, Collider* outCollider );

/*
Renders collisions, for debugging.
*/
void collision_DebugDrawing( Collider* collision, unsigned int camFlags, Color color );
void collision_CollectionDebugDrawing( ColliderCollection collection, unsigned int camFlags, Color color );

// Determines if and where two line segments overlap
//  separate function because we may want to know this without having to create the necessary collision data
bool collision_LineSegmentCollision( Vector2* l0p0, Vector2* l0p1, Vector2* l1p0, Vector2* l1p1, Vector2* outCollPos, float* outT );

// Checks to see if the passed in point is inside the complex polygon defined by the list of points sbPolygon.
//  sbPolygon must be a stretchy buffer.
bool collision_IsPointInsideComplexPolygon( Vector2* pos, Vector2* polygon, size_t numPoints );

#endif
