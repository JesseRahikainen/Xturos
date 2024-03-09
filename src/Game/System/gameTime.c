#include "gameTime.h"

#include <SDL_assert.h>

#include "Math/mathUtil.h"

static float renderDeltaTime = 0.0f;
static float renderNormalizedTime = 0.0f;

void gt_SetRenderTimeDelta( float dt )
{
	renderDeltaTime = dt;
}

float gt_GetRenderTimeDelta( void )
{
	return renderDeltaTime;
}

void gt_SetRenderNormalizedTime( float t )
{
	SDL_assert( t >= 0.0f && t <= 1.0f );
	renderNormalizedTime = clamp01( t );
}

float gt_GetRenderNormalizedTime( void )
{
	return renderNormalizedTime;
}

Uint64 gt_StartTimer( void )
{
	return SDL_GetPerformanceCounter( );
}

float gt_StopTimer( Uint64 t )
{
	return (float)( SDL_GetPerformanceCounter( ) - t ) / (float)SDL_GetPerformanceFrequency( );
}