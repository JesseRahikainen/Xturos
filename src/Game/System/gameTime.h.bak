#ifndef GAME_TIME_H
#define GAME_TIME_H

/* making PHYSICS_TICK to something that will result in a whole number should lead to better results
the second number is how many times per second it will update */
//#define PHYSICS_TICK ( 1000 / 5 )
//#define PHYSICS_TICK ( 1000 / 60 )
//#define PHYSICS_TICK ( 1000 / 15 )
#define PHYSICS_TICK ( 1000 / 30 )
#define PHYSICS_DT ( (float)PHYSICS_TICK / 1000.0f )

void gt_SetRenderTimeDelta( float dt );
float gt_GetRenderTimeDelta( void );

#endif