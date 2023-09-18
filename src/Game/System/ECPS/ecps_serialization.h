#ifndef ECPS_SERIALIZATION_H
#define ECPS_SERIALIZATION_H

#include <stdint.h>
#include <stdbool.h>

#include "ecps_dataTypes.h"

void ecps_InitSerialized( SerializedECPS * serializedECPS );
void ecps_CleanSerialized( SerializedECPS * serializedECPS );

// fills out the serializedECPS structure with all entities in the passed in ecps
void ecps_GenerateSerializedECPS( const ECPS* ecps, SerializedECPS* serializedECPS );

// takes in the serializedECPS and creates the entities in the ecps
void ecps_CreateEntitiesFromSerializedComponents( ECPS* ecps, SerializedECPS* serializedECPS );

// save and load to external files
bool ecps_SaveSerializedECPS( const char* fileName, SerializedECPS* serializedECPS );
bool ecps_LoadSerializedECPS( const char* fileName, SerializedECPS* serializedECPS );

uint32_t ecps_GetLocalIDFromSerializeEntityID( SerializedEntityInfo* sbEntityInfos, EntityID id );
EntityID ecps_GetEntityIDfromSerializedLocalID( SerializedEntityInfo* sbEntityInfos, uint32_t id );

#endif // inclusion guard
