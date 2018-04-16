#ifndef GENERAL_COMPONENTS_H
#define GENERAL_COMPONENTS_H

#include "../System/ECPS/entityComponentProcessSystem.h"
#include "../Math/vector2.h"
#include "../Graphics/color.h"

// general use components that are shared between games
typedef struct {
	Vector2 currPos;
	Vector2 futurePos;
} GCPosData;
extern ComponentID gcPosCompID;

typedef struct {
	Color currClr;
	Color futureClr;
} GCColorData;
extern ComponentID gcClrCompID;

typedef struct {
	float currRot;
	float futureRot;
} GCRotData;
extern ComponentID gcRotCompID;

typedef struct {
	Vector2 currScale;
	Vector2 futureScale;
} GCScaleData;
extern ComponentID gcScaleCompID;

typedef struct {
	int img;
	int8_t depth;
	uint32_t camFlags;
} GCSpriteData;
extern ComponentID gcSpriteCompID;

typedef struct {
	Vector2 halfDim;
} GCAABBCollisionData;
extern ComponentID gcAABBCollCompID;

typedef struct {
	float radius;
} GCCircleCollisionData;
extern ComponentID gcCircleCollCompID;

// used for buttons or anything else that should be clickable
typedef struct {
	Vector2 collisionHalfDim;
	int state;
	int camFlags;
	int32_t priority;
	void (*overResponse)( Entity* btn );
	void (*leaveResponse)( Entity* btn );
	void (*pressResponse)( Entity* btn );
	void (*releaseResponse)( Entity* btn );
} GCPointerResponseData;
extern ComponentID gcPointerResponseCompID;

extern ComponentID gcCleanUpFlagCompID;

void gc_Register( ECPS* ecps );

#endif // inclusion guard