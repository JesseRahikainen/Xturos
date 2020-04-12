#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>
#include <stdbool.h>

// Sending in NULL as the RandomGroup to any of these functions will use a default RandomGroup.

typedef struct {
	uint64_t state[2];
} RandomGroup;

void rand_Seed( RandomGroup* rg, uint32_t seed );

// generic randomization functions
uint8_t rand_GetU8( RandomGroup* rg );
uint16_t rand_GetU16( RandomGroup* rg );
uint32_t rand_GetU32( RandomGroup* rg );
uint64_t rand_GetU64( RandomGroup* rg );

int8_t rand_GetS8( RandomGroup* rg );
int16_t rand_GetS16( RandomGroup* rg );
int32_t rand_GetS32( RandomGroup* rg );
int64_t rand_GetS64( RandomGroup* rg );

// more specialised randomization functions
float rand_GetNormalizedFloat( RandomGroup* rg );
float rand_GetToleranceFloat( RandomGroup* rg, float base, float tol );
int32_t rand_GetToleranceS32( RandomGroup* rg, int32_t base, int32_t tol );
float rand_GetRangeFloat( RandomGroup* rg, float min, float max );
int32_t rand_GetRangeS32( RandomGroup* rg, int32_t min, int32_t max );

bool rand_Choice( RandomGroup* rg );

#endif /* inclusion guard */