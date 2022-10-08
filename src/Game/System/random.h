#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "Math/vector2.h"

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
uint32_t rand_GetRangeU32( RandomGroup* rg, uint32_t min, uint32_t max );
size_t rand_GetArrayEntry( RandomGroup* rg, size_t arraySize );
Vector2* rand_PointInUnitCircle( RandomGroup* rg, Vector2* out );

bool rand_Choice( RandomGroup* rg );

// entropic roller, assuming all chances are in the range [0,1]
//  reduces clumping of successes and failures
//  doesn't work well with 100% chance success
typedef struct {
	RandomGroup* rg;
	float entropy;
} EntropyRoller;

void entropyRoll_Init( EntropyRoller* roller, RandomGroup* rg );
bool entropyRoll_Roll( EntropyRoller* roller, float chanceSuccess );

// used for selecting from a list of weighted choices without knowing
//  the number of elements beforehand.
//  you can run this as long as you want, and whenever you want grab
//  the chosen id
typedef struct {
	RandomGroup* rg;
	uint32_t weightTotal;
	int chosenId;
} InfiniteListSelector;

void infiniteListSelector_Init( InfiniteListSelector* selector, RandomGroup* rg, int invalidId );
void infiniteListSelector_Choose( InfiniteListSelector* selector, int itemId, uint32_t itemWeight );

#endif /* inclusion guard */
