#ifndef GAME_TIME_H
#define GAME_TIME_H

#include <SDL3/SDL_timer.h>

//#define PHYSICS_TICKS_PER_SEC 5
#define PHYSICS_TICKS_PER_SEC 15
//#define PHYSICS_TICKS_PER_SEC 30
//#define PHYSICS_TICKS_PER_SEC 60
#define PHYSICS_TICK ( SDL_GetPerformanceFrequency( ) / PHYSICS_TICKS_PER_SEC )
#define PHYSICS_DT ( (float)PHYSICS_TICK / (float)SDL_GetPerformanceFrequency( ) )

void gt_SetRenderTimeDelta( float dt );
float gt_GetRenderTimeDelta( void );

void gt_SetRenderNormalizedTime( float t );
float gt_GetRenderNormalizedTime( void );

#define TIME_THIS(x) do { Uint64 timer = gt_StartTimer( ); (x); llog( LOG_INFO, " - %s: %.6f", #x, gt_StopTimer( timer ) ); } while(0,0)
#define TIME_THIS_R(r,x) do { Uint64 timer = gt_StartTimer( ); (r)=(x); llog( LOG_INFO, " - %s: %.6f", #x, gt_StopTimer( timer ) ); } while(0,0)

#define START_TIMER( x ) Uint64 timer##x = gt_StartTimer();
#define STOP_AND_LOG_TIMER( x ) float time##x = gt_StopTimer( timer##x ); llog( LOG_INFO, " - %s: %f", #x, time##x );

Uint64 gt_StartTimer( void );
float gt_StopTimer( Uint64 t );

#endif