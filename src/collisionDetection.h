#ifndef JTR_COLLISION_DETECTION_H
#define JTR_COLLISION_DETECTION_H
/*
Detects collisions and calls functions to resolve them.
Based on this:
http://www.metanetsoftware.com/technique/tutorialA.html
*/

#include <stddef.h>
#include "Math/Vector2.h"
#include "Graphics/color.h"

enum ColliderType {
	CT_DEACTIVATED = -1,
	CT_AABB,
	CT_CIRCLE,
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

typedef union {
	enum ColliderType type;
	ColliderAABB aabb;
	ColliderCircle circle;
} Collider;

typedef struct {
	Collider* firstCollider;
	size_t stride;
	int count;
} ColliderCollection;

typedef void(*CollisionResponse)( int firstColliderIdx, int secondColliderIdx, Vector2 separation );

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
Finds if the specified line segment hits anything in the list. Returns 1 if it did, 0 otherwise. Puts the
 collision point into out, if out is NULL it'll exit once it detects any collision instead of finding the first.
*/
int collision_RayCast( Vector2 start, Vector2 end, ColliderCollection collection, Vector2* out );

/*
Renders collisions, for debugging.
*/
void collision_DebugDrawing( Collider* collision, unsigned int camFlags, Color color );
void collision_CollectionDebugDrawing( ColliderCollection collection, unsigned int camFlags, Color color );

#endif
