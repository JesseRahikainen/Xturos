#include "camera.h"

#include <string.h>
#include <assert.h>

typedef struct {
	Vector2 pos;
} CameraState;

typedef struct {
	CameraState start;
	CameraState end;
	unsigned int renderFlags;
	Matrix4 projectionMat;
} Camera;

#define NUM_CAMERAS 16
static Camera cameras[NUM_CAMERAS];

static float currentTime;
static float endTime;

static int currCamera;

/*
Initialize all the cameras, set them to the identity.
*/
void cam_Init( void )
{
	memset( cameras, 0, sizeof( cameras ) );
}

/*
Creates the base projection matrices for all the cameras.
*/
void cam_SetProjectionMatrices( SDL_Window* window )
{
	Matrix4 proj;
	int width, height;
	SDL_GetWindowSize( window, &width, &height );
	mat4_CreateOrthographicProjection( 0.0f, (float)width, 0.0f, (float)height, -1000.0f, 1000.0f, &proj );

	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].projectionMat = proj;
	}
}

/*
Set the state the camera will be at at the end of the next frame.
 Returns <0 if there's a problem.
*/
int cam_SetNextState( int camera, Vector2 pos )
{
	assert( camera < NUM_CAMERAS );

	cameras[camera].end.pos = pos;
	return 0;
}

/*
Takes the current state and adds a value to it to set the next state;
 Returns <0 if there's a problem.
*/
int cam_MoveNextState( int camera, Vector2 delta )
{
	assert( camera < NUM_CAMERAS );
	vec2_Add( &( cameras[camera].start.pos ), &delta, &( cameras[camera].end.pos ) );
	return 0;
}

/*
Do everything that needs to be done when starting a new render set.
*/
void cam_FinalizeStates( float timeToEnd )
{
	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].start = cameras[i].end;
	}
	currentTime = 0.0f;
	endTime = timeToEnd;
}

/*
Update all the cameras, call this before rendering stuff.
*/
void cam_Update( float dt )
{
	currentTime += dt;
}

/*
Gets the view projection matrix for the specified camera.
 Returns <0 if there's a problem.
*/
int cam_GetVPMatrix( int camera, Matrix4* out )
{
	assert( camera < NUM_CAMERAS );

	Vector2 pos;
	Vector2 invPos;
	Matrix4 trans;
	float t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );
	vec2_Lerp( &( cameras[camera].start.pos ), &( cameras[camera].end.pos ), t, &pos );
	vec2_Scale( &pos, -1.0f, &invPos ); // object movement is inverted from camera movement
	mat4_CreateTranslation( invPos.x, invPos.y, 0.0f, &trans );
	mat4_Multiply( &( cameras[camera].projectionMat ), &trans, out );

	return 0;
}

/*
Gets the view matrix for the specified camera.
 Returns <0 if there's a problem.
*/
int cam_GetInverseViewMatrix( int camera, Matrix4* out )
{
	assert( camera < NUM_CAMERAS );

	Vector2 pos;
	float t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );
	vec2_Lerp( &( cameras[camera].start.pos ), &( cameras[camera].end.pos ), t, &pos );
	mat4_CreateTranslation( pos.x, pos.y, 0.0f, out );

	return 0;
}

/*
Turns on render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOnFlags( int camera, unsigned int flags )
{
	assert( camera < NUM_CAMERAS );
	cameras[camera].renderFlags |= flags;
	return 0;
}

/*
Turns off render flags for the camera.
 Returns <0 if there's a problem.
*/
int cam_TurnOffFlags( int camera, unsigned int flags )
{
	assert( camera < NUM_CAMERAS );
	cameras[camera].renderFlags &= ~flags;
	return 0;
}

/*
Gets the flags for the camera.
 Returns 0 if there is something wrong with the index, also when no flags have been set.
*/
unsigned int cam_GetFlags( int camera )
{
	assert( camera < NUM_CAMERAS );
	return cameras[camera].renderFlags;
}

/*
Starts the iteration of all the cameras.
*/
int cam_StartIteration( void )
{
	currCamera = 0;
	
	while( ( currCamera < NUM_CAMERAS ) && ( cameras[currCamera].renderFlags == 0 ) ) {
		++currCamera;
	}

	if( currCamera >= NUM_CAMERAS ) {
		return -1;
	}

	return currCamera;
}

/*
Gets the next active camera. A camera is active if it has any flags on.
 Processing happens in order, so cameras with a higher index will be drawn
 over those with lower.
 Returns the next camera id, returns <0 when there are no more cameras left to process.
*/
int cam_GetNextActiveCam( void )
{
	int nextCamera = currCamera + 1;
	while ( ( nextCamera < NUM_CAMERAS ) && ( cameras[nextCamera].renderFlags == 0 ) ) {
		++nextCamera;
	}

	if( nextCamera >= NUM_CAMERAS ) {
		return -1;
	}

	currCamera = nextCamera;
	return currCamera;
}