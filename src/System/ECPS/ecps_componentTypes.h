#ifndef COMPONENT_TYPES
#define COMPONENT_TYPES

#include <stdint.h>
#include <stdarg.h>

#include "ecps_componentBitFlags.h"
#include "ecps_dataTypes.h"

uint32_t sharedComponent_Enabled;
uint32_t sharedComponent_ID;

ComponentID ecps_ct_AddType( ComponentTypeCollection* ctc, const char* name, size_t size, VerifyComponent verify );
void ecps_ct_Init( ComponentTypeCollection* ctc );
void ecps_ct_CreateBitFlagsVA( ComponentBitFlags* outFlags, int numComponents, va_list list );
void ecps_ct_CreateBitFlags( ComponentBitFlags* outFlags, int numComponents, ... );
void ecps_ct_CleanUp( ComponentTypeCollection* ctc );
size_t ecps_ct_ComponentTypeCount( ComponentTypeCollection* ctc );
size_t ecps_ct_GetComponentTypeSize( ComponentTypeCollection* ctc, size_t idx );
bool ecps_ct_IsComponentTypeValid( ComponentTypeCollection* ctc, ComponentID componentID );

#endif