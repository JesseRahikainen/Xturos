#include "camera.h"

#include <string.h>
#include "Utils/helpers.h"
#include "System/platformLog.h"

typedef struct {
	Vector2 pos;
	float scale;
	Vector2 posOffset;
} CameraState;

typedef struct {
	CameraState start;
	CameraState end;
	uint32_t renderFlags;
	Matrix4 projectionMat;
	Vector2 inverseOffset;
	bool isZeroed; // if the camera is centered at 0, 0 instead of width/2, height/2

	bool isVPMatValid;
	Matrix4 vpMat;

	bool isInvViewMatValid;
	Matrix4 invViewMat;
} Camera;

#define NUM_CAMERAS 16
static Camera cameras[NUM_CAMERAS];

static float currentTime;
static float endTime;

static int currCamera;

static void createStandardProjectionMatrix( int width, int height, Matrix4* outMat )
{
	mat4_CreateOrthographicProjection( 0.0f, (float)width, 0.0f, (float)height, -1000.0f, 1000.0f, outMat );
}

static void createZeroedProjectionMatrix( int width, int height, Matrix4* outMat )
{
	float halfWidth = width / 2.0f;
	float halfHeight = height / 2.0f;
	mat4_CreateOrthographicProjection( -halfWidth, halfWidth, -halfHeight, halfHeight, -1000.0f, 1000.0f, outMat );
}

static Vector2 getCameraViewPosNext( int camera )
{
	Vector2 pos;
	vec2_Add( &( cameras[camera].end.pos ), &( cameras[camera].end.posOffset ), &pos );
	return pos;
}

static Vector2 getCameraViewPosCurr( int camera )
{
	Vector2 pos;
	vec2_Add( &( cameras[camera].start.pos ), &( cameras[camera].start.posOffset ), &pos );
	return pos;
}

// Initialize all the cameras, set them to the identity.
void cam_Init( void )
{
	memset( cameras, 0, sizeof( cameras ) );

	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].start.scale = 1.0f;
		cameras[i].end.scale = 1.0f;

		cameras[i].isVPMatValid = false;
		cameras[i].isInvViewMatValid = false;
	}
}

// Initializes and sets the projection matrices for all the cameras.
void cam_InitProjectionMatrices( int width, int height, bool zeroed )
{
	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].isZeroed = zeroed;
	}

	cam_SetProjectionMatrices( width, height );
}

// Creates the base projection matrices for all the cameras.
//  If zeroed is true it will create matrices with (0,0) being the center, otherwise (0,0) will be the upper left
void cam_SetProjectionMatrices( int width, int height )
{
	Matrix4 standardProjMat;
	Vector2 standardInvOffset;
	createStandardProjectionMatrix( width, height, &standardProjMat );
	standardInvOffset = VEC2_ZERO;

	Matrix4 zeroedProjMat;
	Vector2 zeroedInvOffset;
	createZeroedProjectionMatrix( width, height, &zeroedProjMat );
	zeroedInvOffset = vec2( -width / 2.0f, -height / 2.0f );

	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		if( cameras[i].isZeroed ) {
			cameras[i].projectionMat = zeroedProjMat;
			cameras[i].inverseOffset = zeroedInvOffset;
		} else {
			cameras[i].projectionMat = standardProjMat;
			cameras[i].inverseOffset = standardInvOffset;
		}
		
		cameras[i].isInvViewMatValid = false;
		cameras[i].isVPMatValid = false;
	}
}

// Sets the camera projection matrix with (0,0) being the center
void cam_SetCenteredProjectionMatrix( int cam, int width, int height )
{
	ASSERT_AND_IF_NOT( cam >= 0 ) return;
	ASSERT_AND_IF_NOT( cam < NUM_CAMERAS ) return;
	createZeroedProjectionMatrix( width, height, &( cameras[cam].projectionMat ) );
	cameras[cam].inverseOffset = vec2( -width / 2.0f, -height / 2.0f );
	cameras[cam].isInvViewMatValid = false;
	cameras[cam].isVPMatValid = false;
	cameras[cam].isZeroed = true;
}

// Sets the camera projection matrix with (0,0) being the upper left corner
void cam_SetStandardProjectionMatrix( int cam, int width, int height )
{
	ASSERT_AND_IF_NOT( cam >= 0 ) return;
	ASSERT_AND_IF_NOT( cam < NUM_CAMERAS ) return;
	createStandardProjectionMatrix( width, height, &( cameras[cam].projectionMat ) );
	cameras[cam].inverseOffset = VEC2_ZERO;
	cameras[cam].isInvViewMatValid = false;
	cameras[cam].isVPMatValid = false;
	cameras[cam].isZeroed = false;
}

// Sets a custom projection matrix for a camera
void cam_SetCustomProjectionMatrix( int cam, Matrix4* mat )
{
	ASSERT_AND_IF_NOT( cam >= 0 ) return;
	ASSERT_AND_IF_NOT( cam < NUM_CAMERAS ) return;
	ASSERT_AND_IF_NOT( mat != NULL ) return;

	memcpy( &( cameras[cam].projectionMat ), mat, sizeof( Matrix4 ) );
}

// Directly sets the state of the camera. Only use this if you don't want
//  the camera to lerp between frames.
//  Returns <0 if there's a problem.
int cam_SetState( int camera, Vector2 pos, float scale )
{
	SDL_assert( camera < NUM_CAMERAS );

	cameras[camera].start.pos = pos;
	cameras[camera].start.scale = scale;
	cameras[camera].start.posOffset = VEC2_ZERO;
	cameras[camera].end.pos = pos;
	cameras[camera].end.scale = scale;
	cameras[camera].end.posOffset = VEC2_ZERO;
	return 0;
}

// Set the state the camera will be at at the end of the next frame.
//  Returns <0 if there's a problem.
int cam_SetNextState( int camera, Vector2 pos, float scale )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	vec2_Scale( &pos, scale, &(cameras[camera].end.pos ) );
	//cameras[camera].end.pos = pos;
	cameras[camera].end.scale = scale;
	return 0;
}

// Set the offset for the camera that will be used to modify the position at the end of the next frame.
// // Should only be called after the scale has been set.
//  Returns <0 if there's a problem
int cam_SetNextOffset( int camera, Vector2 offset )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;
	vec2_Scale( &offset, cameras[camera].end.scale, &( cameras[camera].end.posOffset ) );
	return 0;
}

// Set just the position of the cameras next frame.
//  Returns <0 if there's a problem
int cam_SetNextPos( int camera, Vector2 pos )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	vec2_Scale( &pos, cameras[camera].end.scale, &( cameras[camera].end.pos ) );
	return 0;
}

// Takes the current state and adds a value to it to set the next state. This will undo any previous call to cam_MoveNextState
//  Returns <0 if there's a problem.
int cam_MoveNextState( int camera, Vector2 delta, float scaleDelta )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	vec2_Add( &( cameras[camera].start.pos ), &delta, &( cameras[camera].end.pos ) );
	cameras[camera].end.scale = cameras[camera].start.scale + scaleDelta;
	if( cameras[camera].end.scale < 0.0f ) {
		cameras[camera].end.scale = 0.0f;
	}
	return 0;
}

int cam_GetCurrPos( int camera, Vector2* outPos )
{
	ASSERT_AND_IF_NOT( outPos != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	(*outPos) = cameras[camera].start.pos;
	return 0;
}

int cam_GetNextPos( int camera, Vector2* outPos )
{
	ASSERT_AND_IF_NOT( outPos != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	(*outPos) = cameras[camera].end.pos;
	return 0;
}

int cam_GetCurrScale( int camera, float* outScale )
{
	ASSERT_AND_IF_NOT( outScale != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	(*outScale) = cameras[camera].start.scale;
	return 0;
}

int cam_GetNextScale( int camera, float* outScale )
{
	ASSERT_AND_IF_NOT( outScale != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	(*outScale) = cameras[camera].end.scale;
	return 0;
}

// Do everything that needs to be done when starting a new render set.
void cam_FinalizeStates( float timeToEnd )
{
	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].start = cameras[i].end;
		cameras[i].isVPMatValid = false;
		cameras[i].isInvViewMatValid = false;
		cameras[i].end.posOffset = VEC2_ZERO;
	}

	currentTime = 0.0f;
	endTime = timeToEnd;
}

// Update all the cameras, call this before rendering stuff.
void cam_Update( float dt )
{
	for( int i = 0; i < NUM_CAMERAS; ++i ) {
		cameras[i].isVPMatValid = false;
		cameras[i].isInvViewMatValid = false;
	}

	currentTime += dt;
}

// Gets the projection matrix that was set up for the camera.
void cam_GetProjectionMatrix( int camera, Matrix4* out )
{
	ASSERT_AND_IF_NOT( out != NULL ) return;
	ASSERT_AND_IF_NOT( camera >= 0 ) return;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return;

	memcpy( out, &( cameras[camera].projectionMat ), sizeof( Matrix4 ) );
}

// Gets the camera view matrix for the specified camera.
void cam_GetViewMatrix( int camera, Matrix4* out )
{
	ASSERT_AND_IF_NOT( out != NULL ) return;
	ASSERT_AND_IF_NOT( camera >= 0 ) return;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return;

	Vector2 pos;
	Vector2 startPos = getCameraViewPosCurr( camera );
	Vector2 endPos = getCameraViewPosNext( camera );
	Vector2 invPos;
	Matrix4 transTf, scaleTf;
	Matrix4 view;

	float t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );
	vec2_Lerp( &startPos, &endPos, t, &pos );
	vec2_Scale( &pos, -1.0f, &invPos ); // object movement is inverted from camera movement
	mat4_CreateTranslation( invPos.x, invPos.y, 0.0f, &transTf );

	float scale = lerp( cameras[camera].start.scale, cameras[camera].end.scale, t );
	mat4_CreateScale( scale, scale, 1.0f, &scaleTf );

	mat4_Multiply( &transTf, &scaleTf, &view );

	memcpy( out, &(view), sizeof( Matrix4 ) );
}

// Gets the view projection matrix for the specified camera.
//  Returns <0 if there's a problem.
int cam_GetVPMatrix( int camera, Matrix4* out )
{
	ASSERT_AND_IF_NOT( out != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	if( !cameras[camera].isVPMatValid ) {
		Vector2 pos;
		Vector2 startPos = getCameraViewPosCurr( camera );
		Vector2 endPos = getCameraViewPosNext( camera );
		Vector2 invPos;
		Matrix4 transTf, scaleTf;
		Matrix4 view;

		float t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );
		vec2_Lerp( &startPos, &endPos, t, &pos );
		vec2_Scale( &pos, -1.0f, &invPos ); // object movement is inverted from camera movement
		mat4_CreateTranslation( invPos.x, invPos.y, 0.0f, &transTf );

		float scale = lerp( cameras[camera].start.scale, cameras[camera].end.scale, t );
		mat4_CreateScale( scale, scale, 1.0f, &scaleTf );

		//mat4_Multiply( &scaleTf, &transTf, &view );
		mat4_Multiply( &transTf, &scaleTf, &view );

		mat4_Multiply( &( cameras[camera].projectionMat ), &view, &( cameras[camera].vpMat ) );

		cameras[camera].isVPMatValid = true;
	}

	memcpy( out, &( cameras[camera].vpMat ), sizeof( Matrix4 ) );
	
	return 0;
}

// Gets the view matrix for the specified camera.
//  Returns <0 if there's a problem.
int cam_GetInverseViewMatrix( int camera, Matrix4* out )
{
	ASSERT_AND_IF_NOT( out != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	if( !cameras[camera].isInvViewMatValid ) {
		Matrix4 transTf, scaleTf;

		Vector2 startPos = getCameraViewPosCurr( camera );
		Vector2 endPos = getCameraViewPosNext( camera );
		Vector2 pos;
		float t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );
		vec2_Lerp( &startPos, &endPos, t, &pos );
		vec2_Add( &pos, &( cameras[camera].inverseOffset ), &pos );
		mat4_CreateTranslation( pos.x, pos.y, 0.0f, &transTf );

		float scale = 1.0f / lerp( cameras[camera].start.scale, cameras[camera].end.scale, t );
		mat4_CreateScale( scale, scale, 1.0f, &scaleTf );

		memcpy( &( cameras[camera].invViewMat ), &IDENTITY_MATRIX, sizeof( Matrix4 ) );
		mat4_Multiply( &( cameras[camera].invViewMat ), &scaleTf, &( cameras[camera].invViewMat ) );
		mat4_Multiply( &( cameras[camera].invViewMat ), &transTf, &( cameras[camera].invViewMat ) );

		cameras[camera].isInvViewMatValid = true;
	}

	memcpy( out, &( cameras[camera].invViewMat ), sizeof( Matrix4 ) );
	
	return 0;
}

int cam_ScreenPosToWorldPos( int camera, const Vector2* screenPos, Vector2* out )
{
	ASSERT_AND_IF_NOT( screenPos != NULL ) return -1;
	ASSERT_AND_IF_NOT( out != NULL ) return -1;
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	Matrix4 invView;
	cam_GetInverseViewMatrix( camera, &invView );

	Vector2 pos = *screenPos;
	vec2_Add( &pos, &( cameras[camera].inverseOffset ), &pos );
	mat4_TransformVec2Pos( &invView, &pos, out );

	return 0;
}

bool cam_GetWorldBorders( int camera, Vector2* outTopLeft, Vector2* outBottomRight )
{
	ASSERT_AND_IF_NOT( outTopLeft != NULL ) return false;
	ASSERT_AND_IF_NOT( outBottomRight != NULL ) return false;
	ASSERT_AND_IF_NOT( camera >= 0 ) return false;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return false;

	// if we're assuming we're using an orthographic camera here then we can just use this:
	//  width = 2.0f / mat[0], height = 2.0f / mat[5]
	// out->m[0] = 2.0f / rml;
	// out->m[5] = 2.0f / tmb;
	// out->m[10] = -2.0f / fmn;
	float width = 2.0f / cameras[camera].projectionMat.m[0];
	float height = 2.0f / cameras[camera].projectionMat.m[5];

	// given the current position of the camera find the positions that would be at the top left and bottom right corners of the camera
	if( cameras[camera].isZeroed ) {
		// camera positioned at center of the world
		outBottomRight->x = width / 2.0f;
		outBottomRight->y = height / 2.0f;
		outTopLeft->x = -outBottomRight->x;
		outTopLeft->y = -outBottomRight->y;
	} else {
		// camera positioned at top left corner of the world
		(*outTopLeft) = VEC2_ZERO;
		outBottomRight->x = width;
		outBottomRight->y = height;
	}

	return true;
}

// Turns on render flags for the camera.
//  Returns <0 if there's a problem.
int cam_TurnOnFlags( int camera, uint32_t flags )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	cameras[camera].renderFlags |= flags;
	return 0;
}

// Turns off render flags for the camera.
//  Returns <0 if there's a problem.
int cam_TurnOffFlags( int camera, uint32_t flags )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	cameras[camera].renderFlags &= ~flags;
	return 0;
}

// Sets the flags for the camera.
//  Returns <0 if there's a problem.
int cam_SetFlags( int camera, uint32_t flags )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return -1;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return -1;

	cameras[camera].renderFlags = flags;
	return 0;
}

// Gets the flags for the camera.
//  Returns 0 if there is something wrong with the index, also when no flags have been set.
uint32_t cam_GetFlags( int camera )
{
	ASSERT_AND_IF_NOT( camera >= 0 ) return 0;
	ASSERT_AND_IF_NOT( camera < NUM_CAMERAS ) return 0;

	return cameras[camera].renderFlags;
}

// Starts the iteration of all the cameras.
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

// Gets the next active camera. A camera is active if it has any flags on.
//  Processing happens in order, so cameras with a higher index will be drawn
//  over those with lower.
//  Returns the next camera id, returns <0 when there are no more cameras left to process.
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