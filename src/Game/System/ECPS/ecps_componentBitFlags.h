#ifndef COMPONENT_BIT_FLAGS_H
#define COMPONENT_BIT_FLAGS_H

#include <stdbool.h>
#include <stdint.h>

#include "ecps_dataTypes.h"

#include "ecps_values.h"

void ecps_cbf_SetFlagOn( ComponentBitFlags* flags, uint32_t flagToSet );
void ecps_cbf_SetFlagOff( ComponentBitFlags* flags, uint32_t flagToUnset );
bool ecps_cbf_IsFlagOn( const ComponentBitFlags* flags, uint32_t flagToTest );
bool ecps_cbf_CompareExact( const ComponentBitFlags* test, const ComponentBitFlags* against );
bool ecps_cbf_CompareContains( const ComponentBitFlags* test, const ComponentBitFlags* against );

#endif