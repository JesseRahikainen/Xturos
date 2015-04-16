#ifndef CAMERA_H
#define CAMERA_H

#include <SDL.h>

#include "../Math/vector2.h"
#include "../Math/matrix4.h"

/*
Initialize all the cameras, set them to the identity.
*/
void cam_Init( void );

/*
Creates the base projection matrices for all the cameras.
*/
void cam_SetProjectionMatrices( SDL_Window* window );

/*
Set the state the camera will be at at the end of the next frame.
 Returns <0 if there's a problem.
*/
int cam_SetNextState( int camera, Vector2 pos );

/*
Takes the current state and adds a value to it to set the next state;
 Returns <0 if there's a problem.
*/
int cam_MoveNextState( int camera, Vector2 delta );

/*
Do everything that needs to be done when starting a new render set.
*/
void cam_FinalizeStates( float timeToEnd );

/*
Update all the cameras, call this before rendering stuff.
*/
void cam_Update( float dt );

/*
s
 Returns <0 if there's a problem.
*/
int cam_GetVPMatrix( int camera, Matrix4* out );

/*
Gets the view matrix for the specified camera.
 Returns <0 if there's a problem.
*/
int cam_GetInverseViewMatrix( int camera, Matrix4* out );

/*
Turns on render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOnFlags( int camera, unsigned int flags );

/*
Turns off render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOffFlags( int camera, unsigned int flags );

/*
Gets the flags for the camera.
 Returns 0 if there is something wrong with the index, also when no flags have been set.
*/
unsigned int cam_GetFlags( int camera );

/*
Starts the iteration of all the cameras.
*/
int cam_StartIteration( void );

/*
Gets the next active camera. A camera is active if it has any flags on.
 Processing happens in order, so cameras with a higher index will be drawn
 over those with lower.
 Returns the next camera id, returns <0 when there are no more cameras left to process.
*/
int cam_GetNextActiveCam( void );

#endif /* inclusion guard */