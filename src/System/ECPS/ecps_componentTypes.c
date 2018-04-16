#include "ecps_componentTypes.h"

#include <stdlib.h>
#include <string.h>

#include "../../Utils/stretchyBuffer.h"

#include "ecps_values.h"
#include "ecps_componentBitFlags.h"

ComponentID ecps_ct_AddType( ComponentTypeCollection* ctc, const char* name, size_t size, VerifyComponent verify )
{
	assert( sb_Count( ctc->sbTypes ) < MAX_NUM_COMPONENT_TYPES );

	ComponentType newType;

	newType.size = size;
	newType.verify = verify;

	if( name != NULL ) {
		strncpy( newType.name, name, sizeof( newType.name ) - 1 );
		newType.name[sizeof( newType.name ) - 1] = 0;
	}

	uint32_t id = sb_Count( ctc->sbTypes );
	sb_Push( ctc->sbTypes, newType );

	return id;
}

void ecps_ct_Init( ComponentTypeCollection* ctc )
{
	ctc->sbTypes = NULL;

	sharedComponent_ID = ecps_ct_AddType( ctc, "S_ID", sizeof( uint32_t ), NULL );
	sharedComponent_Enabled = ecps_ct_AddType( ctc, "S_ENABLED", 0, NULL );
}

// helper function to create bit flag sets
void ecps_ct_CreateBitFlagsVA( ComponentBitFlags* outFlags, int numComponents, va_list list )
{
	assert( outFlags != NULL );

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
	assert( idx < sb_Count( ctc->sbTypes ) );

	return ctc->sbTypes[idx].size;
}

bool ecps_ct_IsComponentTypeValid( ComponentTypeCollection* ctc, ComponentID componentID )
{
	return ( componentID < sb_Count( ctc->sbTypes ) );
}