#ifndef JTR_COLLISION_DETECTION_H
#define JTR_COLLISION_DETECTION_H
/*
Detects collisions and calls functions to resolve them.
Based on this:
http://www.metanetsoftware.com/technique/tutorialA.html
*/

#include <stddef.h>
#include "Math/Vector2.h"

enum ColliderType {
	CT_AABB,
	CT_CIRCLE,
	NUM_COLLIDER_TYPES
};

struct ColliderAABB {
	enum ColliderType type;
	Vector2 center;
	Vector2 halfDim;
};

struct ColliderCircle {
	enum ColliderType type;
	Vector2 center;
	float radius;
};

union Collider {
	enum ColliderType type;
	struct ColliderAABB aabb;
	struct ColliderCircle circle;
};

typedef void(*CollisionResponse)( int collideeIdx, int colliderIdx, Vector2 separation );

/*
Finds and handles the all collisions between mainCollider and the colliders in the list.
 NOTE: You shouldn't modify the passed in list in the response.
*/
void detectCollisions( union Collider* mainCollider, union Collider* listFirst, size_t listStride, int listCount,
	CollisionResponse response, int passThroughIdx );

/*
Finds if the specified line segment hits anything in the list. Returns 1 if it did, 0 otherwise. Puts the
 collision point into out, if out is NULL it'll exit once it detects any collision instead of finding the first.
*/
int detectRayCastHit( Vector2 start, Vector2 end, union Collider* listFirst, size_t listStride, int listCount, Vector2* out );

/*
Renders all the collider boxes.
*/
void collisionDebugRendering( union Collider* firstCollision, unsigned int camFlags, size_t stride, int count,
	char red, char green, char blue );

#endif
