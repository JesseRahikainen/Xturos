#ifndef GAME_TIME_H
#define GAME_TIME_H

#include <SDL_timer.h>

/* making PHYSICS_TICK to something that will result in a whole number should lead to better results
the second number is how many times per second it will update */
//#define PHYSICS_TICK ( 1000 / 5 )
//#define PHYSICS_TICK ( 1000 / 60 )
//#define PHYSICS_TICK ( 1000 / 15 )
#define PHYSICS_TICK ( SDL_GetPerformanceFrequency( ) / 30 )
#define PHYSICS_DT ( (float)PHYSICS_TICK / (float)SDL_GetPerformanceFrequency( ) )

void gt_SetRenderTimeDelta( float dt );
float gt_GetRenderTimeDelta( void );

#define TIME_THIS(x) do { Uint64 timer = gt_StartTimer( ); (x); llog( LOG_INFO, " - %s: %.6f", #x, gt_StopTimer( timer ) ); } while(0,0)
#define TIME_THIS_R(r,x) do { Uint64 timer = gt_StartTimer( ); (r)=(x); llog( LOG_INFO, " - %s: %.6f", #x, gt_StopTimer( timer ) ); } while(0,0)

Uint64 gt_StartTimer( void );
float gt_StopTimer( Uint64 t );

#endif