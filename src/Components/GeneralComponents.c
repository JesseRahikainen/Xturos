#include "GeneralComponents.h"

ComponentID gcPosCompID = INVALID_COMPONENT_ID;
ComponentID gcClrCompID = INVALID_COMPONENT_ID;
ComponentID gcRotCompID = INVALID_COMPONENT_ID;
ComponentID gcScaleCompID = INVALID_COMPONENT_ID;
ComponentID gcSpriteCompID = INVALID_COMPONENT_ID;
ComponentID gc3x3SpriteCompID = INVALID_COMPONENT_ID;
ComponentID gcAABBCollCompID = INVALID_COMPONENT_ID;
ComponentID gcCircleCollCompID = INVALID_COMPONENT_ID;
ComponentID gcPointerResponseCompID = INVALID_COMPONENT_ID;
ComponentID gcCleanUpFlagCompID = INVALID_COMPONENT_ID;
ComponentID gcTextCompID = INVALID_COMPONENT_ID;

void gc_Register( ECPS* ecps )
{
	gcPosCompID = ecps_AddComponentType( ecps, "GC_POS", sizeof( GCPosData ), NULL );
	gcClrCompID = ecps_AddComponentType( ecps, "GC_CLR", sizeof( GCColorData ), NULL );
	gcRotCompID = ecps_AddComponentType( ecps, "GC_ROT", sizeof( GCRotData ), NULL );
	gcScaleCompID = ecps_AddComponentType( ecps, "GC_SCL", sizeof( GCScaleData ), NULL );
	gcSpriteCompID = ecps_AddComponentType( ecps, "GC_SPRT", sizeof( GCSpriteData ), NULL );
	gc3x3SpriteCompID = ecps_AddComponentType( ecps, "GC_3x3", sizeof( GC3x3SpriteData ), NULL );
	gcAABBCollCompID = ecps_AddComponentType( ecps, "GC_AABB", sizeof( GCAABBCollisionData ), NULL );
	gcCircleCollCompID = ecps_AddComponentType( ecps, "GC_CIRCLE", sizeof( GCCircleCollisionData ), NULL );
	gcPointerResponseCompID = ecps_AddComponentType( ecps, "GC_CLICK", sizeof( GCPointerResponseData ), NULL );
	gcCleanUpFlagCompID = ecps_AddComponentType( ecps, "GC_DEAD", 0, NULL );
	gcTextCompID = ecps_AddComponentType( ecps, "GC_TXT", sizeof( GCTextData ), NULL );
}