#ifndef CAMERA_H
#define CAMERA_H

#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>

#include "../Math/vector2.h"
#include "../Math/matrix4.h"

/*
Initialize all the cameras, set them to the identity.
*/
void cam_Init( void );

/*
Creates the base projection matrices for all the cameras.
 If zeroed is true it will create matrices with (0,0) being the center, otherwise (0,0) will be the upper left
*/
void cam_SetProjectionMatrices( int width, int height, bool zeroed );

/*
Sets the camera projection matrix with (0,0) being the center
*/
void cam_SetCenteredProjectionMatrix( int cam, int width, int height );

/*
Sets the camera projection matrix with (0,0) being the upper left corner
*/
void cam_SetStandardProjectionMatrix( int cam, int width, int height );

/*
Sets a custom projection matrix for a camera
*/
void cam_SetCustomProjectionMatrix( int cam, Matrix4* mat );

/*
Directly sets the state of the camera. Only use this if you don't want
 the camera to lerp between frames.
 Returns <0 if there's a problem.
*/
int cam_SetState( int camera, Vector2 pos, float scale );

/*
Set the state the camera will be at at the end of the next frame.
 Returns <0 if there's a problem.
*/
int cam_SetNextState( int camera, Vector2 pos, float scale );

/*
Takes the current state and adds a value to it to set the next state;
 Returns <0 if there's a problem.
*/
int cam_MoveNextState( int camera, Vector2 delta, float scaleDelta );

int cam_GetCurrPos( int camera, Vector2* outPos );
int cam_GetNextPos( int camera, Vector2* outPos );
int cam_GetCurrScale( int camera, float* outScale );
int cam_GetNextScale( int camera, float* outScale );

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

int cam_ScreenPosToWorldPos( int camera, const Vector2* screenPos, Vector2* out );

/*
Turns on render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOnFlags( int camera, uint32_t flags );

/*
Turns off render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOffFlags( int camera, uint32_t flags );

/*
Gets the flags for the camera.
 Returns 0 if there is something wrong with the index, also when no flags have been set.
*/
uint32_t cam_GetFlags( int camera );

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