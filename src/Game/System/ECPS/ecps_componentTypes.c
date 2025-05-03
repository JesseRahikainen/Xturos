#include "ecps_componentTypes.h"

#include <stdlib.h>
#include <string.h>

#include "Utils/stretchyBuffer.h"

#include "ecps_values.h"
#include "ecps_componentBitFlags.h"

#include "Utils/helpers.h"

uint32_t sharedComponent_Enabled;
uint32_t sharedComponent_ID;

// TODO: Why is this here when we're doing the same things in entityComponentProcessSystem.c?
ComponentID ecps_ct_AddType( ComponentTypeCollection* ctc, const char* name, uint32_t version, size_t size, size_t align, CleanUpComponent cleanUp, VerifyComponent verify )
{
	SDL_assert( sb_Count( ctc->sbTypes ) < MAX_NUM_COMPONENT_TYPES );

	ComponentType newType;

	newType.size = size;
	newType.align = align;
	newType.verify = verify;
	newType.cleanUp = cleanUp;
	newType.version = version;
	newType.serialize = NULL;
	newType.deserialize = NULL;

	if( name != NULL ) {
		strncpy( newType.name, name, sizeof( newType.name ) - 1 );
		newType.name[sizeof( newType.name ) - 1] = 0;
	}

	ComponentID id = (ComponentID)sb_Count( ctc->sbTypes );
	sb_Push( ctc->sbTypes, newType );

	return id;
}

void ecps_ct_Init( ComponentTypeCollection* ctc )
{
	ctc->sbTypes = NULL;

	sharedComponent_ID = ecps_ct_AddType( ctc, "S_ID", 0, sizeof( uint32_t ), ALIGN_OF( uint32_t ), NULL, NULL );
	sharedComponent_Enabled = ecps_ct_AddType( ctc, "S_ENABLED", 0, 0, 0, NULL, NULL );
}

// helper function to create bit flag sets
void ecps_ct_CreateBitFlagsVA( ComponentBitFlags* outFlags, int numComponents, va_list list )
{
	SDL_assert( outFlags != NULL );

	memset( outFlags, 0, sizeof( ComponentBitFlags ) );
	for( int i = 0; i < numComponents; ++i ) {
		ecps_cbf_SetFlagOn( outFlags, va_arg( list, uint32_t ) );
	}
}

// helper function to create bit flag sets
void ecps_ct_CreateBitFlags( ComponentBitFlags* outFlags, int numComponents, ... )
{
	va_list list;
	va_start( list, numComponents );
	ecps_ct_CreateBitFlagsVA( outFlags, numComponents, list );
	va_end( list );
}

void ecps_ct_CleanUp( ComponentTypeCollection* ctc )
{
	sb_Release( ctc->sbTypes );
}

size_t ecps_ct_ComponentTypeCount( ComponentTypeCollection* ctc )
{
	return sb_Count( ctc->sbTypes );
}

size_t ecps_ct_GetComponentTypeSize( ComponentTypeCollection* ctc, size_t idx )
{
	ASSERT_AND_IF_NOT( idx < sb_Count( ctc->sbTypes ) ) return 0;

	return ctc->sbTypes[idx].size;
}

size_t ecps_ct_GetComponentTypeAlign( ComponentTypeCollection* ctc, size_t idx )
{
	ASSERT_AND_IF_NOT( idx < sb_Count( ctc->sbTypes ) ) return 0;

	return ctc->sbTypes[idx].align;
}

uint32_t ecps_ct_GetComponentTypeVersion( ComponentTypeCollection* ctc, size_t idx )
{
	ASSERT_AND_IF_NOT( idx < sb_Count( ctc->sbTypes ) ) return UINT32_MAX;

	return ctc->sbTypes[idx].version;
}

bool ecps_ct_IsComponentTypeValid( ComponentTypeCollection* ctc, ComponentID componentID )
{
	return ( componentID < sb_Count( ctc->sbTypes ) );
}
