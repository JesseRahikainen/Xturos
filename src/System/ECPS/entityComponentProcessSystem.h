#ifndef ENTITY_COMPONENT_PROCESS_SYSTEM_H
#define ENTITY_COMPONENT_PROCESS_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#include "ecps_dataTypes.h"

// Sets up the ecps, ready to have components, processes, and entities created
void ecps_StartInitialization( ECPS* ecps );

// Switches states, no way to change back to the initialization state
void ecps_FinishInitialization( ECPS* ecps );

// Clean up all the resources
void ecps_CleanUp( ECPS* ecps );

// adds a component type and returns the id to reference it by
//  this can only be done before
ComponentID ecps_AddComponentType( ECPS* ecps, const char* name, size_t size, VerifyComponent verify );

// this attempts to set up a process to be used by the passed in ecps
bool ecps_CreateProcess( ECPS* ecps,
	const char* name, PreProcFunc preProc, ProcFunc proc, PostProcFunc postProc,
	Process* outProcess, size_t numComponents, ... );

// run a process, must have been created with the associated entity-component-process system
void ecps_RunProcess( ECPS* ecps, Process* process );

// creates an entity with the associated components, excepts the variable argument list to be
//  interleaved { ComponentID id, void* compData } groupings
//  the memory pointed to by compData is copied into the component specified by id for the
//  new entity
//  returns the id of the entity
//  the returned id is 0 if the creation fails
EntityID ecps_CreateEntity( ECPS* ecps, size_t numComponents, ... );

// finds the entity with the given id
bool ecps_GetEntityByID( ECPS* ecps, EntityID entityID, Entity* outEntity );

// adds a component to the specified entity
//  if the entity already has this component this acts like an expensive ecps_GetComponentFromEntity( )
//  returns >= 0 if successful, < 0 if unsuccesful
//  data should be a pointer to the data used to initialize the new component, if it's NULL it will set whatever
//   memory is created for the component to 0
//  modifies the passed in Entity to match the new structure
//  returns whether the addition was a success or not
int ecps_AddComponentToEntity( ECPS* ecps, Entity* entity, ComponentID componentID, void* data );

// acts as ecps_AddComponentToEntity( ), but uses an EntityID instead of an Entity structure
int ecps_AddComponentToEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID, void* data );

// removed a component from the specified entity
//  if the entity doesn't have this component it will do nothing
//  returns >= 0 if successful, < 0 if unsuccesful
//  modifies the passed in Entity to match the new structure
int ecps_RemoveComponentFromEntity( ECPS* ecps, Entity* entity, ComponentID componentID );

// acts as ecps_RemoveComponentFromEntity( ), but uses an EntityID instead of an Entity structure
int ecps_RemoveComponentFromEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID );

bool ecps_DoesEntityHaveComponent( const Entity* entity, ComponentID componentID );
bool ecps_DoesEntityHaveComponentByID( ECPS* ecps, EntityID entityID, ComponentID componentID );

// gets the component from the entity, puts a pointer to it in outData
//  puts in NULL if the entity doesn't have that component
bool ecps_GetComponentFromEntity( const Entity* entity, ComponentID componentID, void** outData );
bool ecps_GetComponentFromEntityByID( ECPS* ecps, EntityID entityID, ComponentID componentID, void** outData );
bool ecps_GetEntityAndComponentByID( ECPS* ecps, EntityID entityID, ComponentID componentID, Entity* outEntity, void** outData );
void ecps_DestroyEntity( ECPS* ecps, const Entity* entity );
void ecps_DestroyEntityByID( ECPS* ecps, EntityID entityID );

// clears out all entities, not ids will be valid after this is called
void ecps_DestroyAllEntities( ECPS* ecps );

// debugging stuff
void ecps_DumpAllEntities( ECPS* ecps, const char* tag );

#endif