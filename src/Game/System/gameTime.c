#include "gameTime.h"

static float renderDeltaTime = 0.0f;

void gt_SetRenderTimeDelta( float dt )
{
	renderDeltaTime = dt;
}

float gt_GetRenderTimeDelta( void )
{
	return renderDeltaTime;
}

Uint64 gt_StartTimer( void )
{
	return SDL_GetPerformanceCounter( );
}

float gt_StopTimer( Uint64 t )
{
	return (float)( SDL_GetPerformanceCounter( ) - t ) / (float)SDL_GetPerformanceFrequency( );
}