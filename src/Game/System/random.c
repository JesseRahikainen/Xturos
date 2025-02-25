#include "random.h"

#include <stdlib.h>
#include <math.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL.h>

#include "Math/mathUtil.h"

static RandomGroup defaultRG;

#define CHECK( rg ) ( ( ( rg ) == NULL ) ? ( rg ) = &defaultRG : 0 )

// using the Xoroshiro128+ algorithm, based on the code from here: http://vigna.di.unimi.it/xorshift/xoroshiro128plus.c
//  is fast and gives good results


#define ROTL( x, k ) ( ( x ) << ( k ) | ( ( x ) >> ( 64 - ( k ) ) ) )

static uint64_t next( RandomGroup* rg )
{
	CHECK( rg );

	const uint64_t s0 = rg->state[0];
	uint64_t s1 = rg->state[1];
	const uint64_t result = s0 + s1;

	s1 ^= s0;
	rg->state[0] = ROTL( s0, 55 ) ^ s1 & ( s1 << 14 );
	rg->state[1] = ROTL( s1, 36 );

	return result;
}

static uint64_t splitmix64( uint64_t* state )
{
	uint64_t z = ( ( *state ) += UINT64_C( 0x9E3779B97F4A7C15 ) );
	z = ( z ^ ( z >> 30 ) ) * UINT64_C( 0xBF58476D1CE4E5B9 );
	z = ( z ^ ( z >> 27 ) ) * UINT64_C( 0x94D049BB133111EB );
	return z ^ ( z >> 31 );
}

void rand_Seed( RandomGroup* rg, uint32_t seed )
{
	CHECK( rg );

	// generate state based on seed, just use the splitmix64 algorithm to generate the initial state
	uint64_t state = seed;
	rg->state[0] = splitmix64( &state );
	rg->state[1] = splitmix64( &state );
}

// avoids various issues when generating large amounts of random numbers
//  useful when you can't be sure range is a factor of UINT64_MAX
static uint64_t getBalancedRandom( RandomGroup* rg, uint64_t range )
{
	uint64_t fullRanges = UINT64_MAX / range;
	uint64_t maxRoll = range * fullRanges;
	uint64_t roll;
	do {
		roll = next( rg );
	} while( roll > maxRoll );
	return roll % range;
}

// generic randomization functions
uint8_t rand_GetU8( RandomGroup* rg )
{
	return (uint8_t)( next( rg ) % UINT8_MAX );
}

uint16_t rand_GetU16( RandomGroup* rg )
{
	return (uint16_t)( next( rg ) % UINT16_MAX );
}

uint32_t rand_GetU32( RandomGroup* rg )
{
	return (uint32_t)( next( rg ) % UINT32_MAX );
}

uint64_t rand_GetU64( RandomGroup* rg )
{
	return next( rg );
}

int8_t rand_GetS8( RandomGroup* rg )
{
	return (int8_t)rand_GetU8( rg );
}

int16_t rand_GetS16( RandomGroup* rg )
{
	return (int16_t)rand_GetU16( rg );
}

int32_t rand_GetS32( RandomGroup* rg )
{
	return (int32_t)rand_GetU32( rg );
}

int64_t rand_GetS64( RandomGroup* rg )
{
	return (int64_t)next( rg );
}

// more specialised randomization functions
float rand_GetNormalizedFloat( RandomGroup* rg )
{
	double val = (double)( next( rg ) ) / (double)UINT64_MAX;
	return (float)val;
}

float rand_GetToleranceFloat( RandomGroup* rg, float base, float tol )
{
	return rand_GetRangeFloat( rg, ( base - tol ), ( base + tol ) );
}

int32_t rand_GetToleranceS32( RandomGroup* rg, int32_t base, int32_t tol )
{
	return rand_GetRangeS32( rg, ( base - tol ), ( base + tol ) );
}

float rand_GetRangeFloat( RandomGroup* rg, float min, float max )
{
	return lerp( min, max, rand_GetNormalizedFloat( rg ) );
}

int32_t rand_GetRangeS32( RandomGroup* rg, int32_t min, int32_t max )
{
	int32_t range = ( max - min ) + 1;
	if( range <= 0 ) {
		return min;
	}
	return min + ( (int32_t)getBalancedRandom( rg, range ) );
}

uint32_t rand_GetRangeU32( RandomGroup* rg, uint32_t min, uint32_t max )
{
	SDL_assert( min <= max );
	uint32_t range = ( max - min );
	if( range <= 0 ) {
		return min;
	}
	range += 1;
	return min + ( (uint32_t)getBalancedRandom( rg, range ) );
}

size_t rand_GetArrayEntry( RandomGroup* rg, size_t arraySize )
{
	return (size_t)getBalancedRandom( rg, (uint64_t)arraySize );
}

size_t rand_GetArrayEntryInRange( RandomGroup* rg, size_t min, size_t max )
{
	SDL_assert( min <= max );
	size_t range = ( max - min );
	if( range <= 0 ) {
		return min;
	}
	range += 1;
	return min + rand_GetArrayEntry( rg, range );
}

Vector2* rand_PointInUnitCircle( RandomGroup* rg, Vector2* out )
{
	SDL_assert( out != NULL );

	// todo: test this versus rejection sampling to see which is faster
	float d = sqrtf( rand_GetNormalizedFloat( rg ) );
	float r = rand_GetRangeFloat( rg, 0.0f, M_TWO_PI_F );

	out->x = d * SDL_cosf( r );
	out->y = d * SDL_sinf( r );

	return out;
}

bool rand_Choice( RandomGroup* rg )
{
	return (bool)( next( rg ) % 2 );
}

void entropyRoll_Init( EntropyRoller* roller, RandomGroup* rg )
{
	SDL_assert( roller != NULL );

	roller->rg = rg;
	roller->entropy = rand_GetNormalizedFloat( rg );
}

bool entropyRoll_Roll( EntropyRoller* roller, float chanceSuccess )
{
	SDL_assert( roller != NULL );
	SDL_assert( chanceSuccess >= 0.0f );
	SDL_assert( chanceSuccess <= 1.0f );

	if( chanceSuccess >= 1.0f ) {
		return true;
	} else if( chanceSuccess <= 0.0f ) {
		return false;
	}

	bool ret = false;
	roller->entropy += rand_GetRangeFloat( roller->rg, 0.0f, chanceSuccess ) + rand_GetRangeFloat( roller->rg, 0.0f, chanceSuccess );
	if( roller->entropy >= 1.0f ) {
		ret = true;
		roller->entropy -= 1.0f;
	}

	return ret;
}

void infiniteListSelector_Init( InfiniteListSelector* selector, RandomGroup* rg, int invalidId )
{
	SDL_assert( selector != NULL );

	selector->chosenId = invalidId;
	selector->rg = rg;
	selector->weightTotal = 0;
}

void infiniteListSelector_Choose( InfiniteListSelector* selector, int itemId, uint32_t itemWeight )
{
	SDL_assert( selector != NULL );

	if( itemWeight == 0 ) return;

	uint32_t totalWeight = selector->weightTotal + itemWeight;
	if( rand_GetRangeU32( selector->rg, 0, totalWeight ) > selector->weightTotal ) {
		selector->chosenId = itemId;
	}
	selector->weightTotal = totalWeight;
}