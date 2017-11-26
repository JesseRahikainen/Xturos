#include "ecps_componentBitFlags.h"

#include <stdlib.h>
#include <assert.h>

void ecps_cbf_SetFlagOn( ComponentBitFlags* flags, uint32_t flagToSet )
{
	assert( flags != NULL );
	assert( flagToSet < MAX_NUM_COMPONENT_TYPES );

	uint32_t idx = flagToSet / 32;
	uint32_t idxBit = flagToSet - ( idx * 32 );

	flags->bits[idx] |= ( 1 << idxBit );
}

void ecps_cbf_SetFlagOff( ComponentBitFlags* flags, uint32_t flagToUnset )
{
	assert( flags != NULL );
	assert( flagToUnset < MAX_NUM_COMPONENT_TYPES );

	uint32_t idx = flagToUnset / 32;
	uint32_t idxBit = flagToUnset - ( idx * 32 );

	flags->bits[idx] &= ~( 1 << idxBit );
}

bool ecps_cbf_IsFlagOn( const ComponentBitFlags* flags, uint32_t flagToTest )
{
	assert( flags != NULL );
	assert( flagToTest < MAX_NUM_COMPONENT_TYPES );

	uint32_t idx = flagToTest / 32;
	uint32_t idxBit = flagToTest - ( idx * 32 );

	return ( ( flags->bits[idx] & ( 1 << idxBit ) ) != 0 );
}

bool ecps_cbf_CompareExact( const ComponentBitFlags* test, const ComponentBitFlags* against )
{
	assert( test != NULL );
	assert( against != NULL );

	for( int i = 0; i < FLAGS_ARRAY_SIZE; ++i ) {
		if( test->bits[i] != against->bits[i] ) {
			return false;
		}
	}

	return true;
}

bool ecps_cbf_CompareContains( const ComponentBitFlags* test, const ComponentBitFlags* against )
{
	assert( test != NULL );
	assert( against != NULL );

	for( int i = 0; i < FLAGS_ARRAY_SIZE; ++i ) {
		if( ( test->bits[i] & against->bits[i] ) != test->bits[i] ) {
			return false;
		}
	}

	return true;
}