#include "testSteeringScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../Utils/stretchyBuffer.h"
#include "../Input/input.h"
#include "../System/random.h"

#include <math.h>
#include <assert.h>

typedef struct {
	Vector2 pos;
	Vector2 vel;
	float maxSpeed;
	Vector2 linearAccel;

	float rotRad;
	float rotSpdRad;
	float maxRotSpeedRad;
	float angularAccelRad;


	Vector2 wanderTarget;

	float radius;
	Color clr;
} SteeringVehicle;

static SteeringVehicle* sbVehicles = NULL;

static Vector2 bordersMin;
static Vector2 bordersMax;

static Vector2 testTarget = { 0.0f, 0.0f };

static void createVehicle( Vector2 pos, Color clr )
{
	SteeringVehicle v;

	v.pos = pos;
	v.vel = vec2( 0.0f, 0.0f );
	v.maxSpeed = 200.0f;

	v.rotRad = 0.0f;
	v.rotSpdRad = DEG_TO_RAD( 0.0f );
	v.maxRotSpeedRad = DEG_TO_RAD( 720.0f );

	v.linearAccel = VEC2_ZERO;
	v.angularAccelRad = 0.0f;

	v.radius = 10.0f;
	v.clr = clr;

	v.wanderTarget = pos;

	sb_Push( sbVehicles, v );
}

static void drawVehicles( )
{
	for( size_t i = 0; i < sb_Count( sbVehicles ); ++i ) {
		debugRenderer_Circle( 1, sbVehicles[i].pos, sbVehicles[i].radius, sbVehicles[i].clr );

		// draw line from the center to the edge for the facing
		Vector2 facing;
		vec2_NormalFromRot( sbVehicles[i].rotRad, &facing );
		vec2_AddScaled( &( sbVehicles[i].pos ), &facing, sbVehicles[i].radius, &facing );
		debugRenderer_Line( 1, sbVehicles[i].pos, facing, sbVehicles[i].clr );

		// draw a line outward from the edge for the velocity direction
		Vector2 normalVel = sbVehicles[i].vel;
		Vector2 velLineStart;
		Vector2 velLineEnd;
		float mag = vec2_Normalize( &normalVel );
		vec2_AddScaled( &( sbVehicles[i].pos ), &normalVel, sbVehicles[i].radius, &velLineStart );
		vec2_AddScaled( &velLineStart, &normalVel, ( mag / sbVehicles[i].maxSpeed ) * sbVehicles[i].radius, &velLineEnd );
		debugRenderer_Line( 1, velLineStart, velLineEnd, sbVehicles[i].clr );
	}
}

static void vehiclePhysics( float dt )
{
	for( size_t i = 0; i < sb_Count( sbVehicles ); ++i ) {

		vec2_AddScaled( &( sbVehicles[i].pos ), &( sbVehicles[i].vel ), dt, &( sbVehicles[i].pos ) );
		vec2_AddScaled( &( sbVehicles[i].pos ), &( sbVehicles[i].linearAccel ), 0.5f * dt * dt, &( sbVehicles[i].pos ) );

		sbVehicles[i].rotRad += ( sbVehicles[i].rotSpdRad * dt ) + ( 0.5f * dt * dt * sbVehicles[i].angularAccelRad );

		vec2_AddScaled( &( sbVehicles[i].vel ), &( sbVehicles[i].linearAccel ), dt, &( sbVehicles[i].vel ) );
		if( vec2_MagSqrd( &(sbVehicles[i].vel) ) > ( sbVehicles[i].maxSpeed * sbVehicles[i].maxSpeed ) ) {
			vec2_Normalize( &(sbVehicles[i].vel) );
			vec2_Scale( &(sbVehicles[i].vel), sbVehicles[i].maxSpeed, &(sbVehicles[i].vel) );
		}

		sbVehicles[i].rotSpdRad += sbVehicles[i].angularAccelRad * dt;
		if( fabsf( sbVehicles[i].rotSpdRad ) > sbVehicles[i].maxRotSpeedRad ) {
			sbVehicles[i].rotSpdRad = sbVehicles[i].maxRotSpeedRad * sign( sbVehicles[i].rotSpdRad );
		}

		sbVehicles[i].pos.x = clamp( bordersMin.x + sbVehicles[i].radius, bordersMax.x - sbVehicles[i].radius, sbVehicles[i].pos.x );
		sbVehicles[i].pos.y = clamp( bordersMin.y + sbVehicles[i].radius, bordersMax.y - sbVehicles[i].radius, sbVehicles[i].pos.y );
	}
}

//************************************
// Basic Steering Behaviors
//  For these we just return the desired velocity, the magnitude represents how much of their max speed they will want to use
void steering_Seek( Vector2* pos, Vector2* target, Vector2* out )
{
	assert( pos != NULL );
	assert( target != NULL );
	assert( out != NULL );

	vec2_Subtract( target, pos, out );
	vec2_Normalize( out );
}

void steering_Flee( Vector2* pos, Vector2* target, Vector2* out )
{
	assert( pos != NULL );
	assert( target != NULL );
	assert( out != NULL );

	vec2_Subtract( pos, target, out );
	vec2_Normalize( out );
}

void steering_Arrive( Vector2* pos, Vector2* target, float innerRadius, float outerRadius, Vector2* out )
{
	assert( pos != NULL );
	assert( target != NULL );
	assert( out != NULL );
	assert( innerRadius <= outerRadius );

	vec2_Subtract( target, pos, out );
	float dist = vec2_Normalize( out );

	float speed = inverseLerp( innerRadius, outerRadius, dist );
	vec2_Scale( out, speed, out );
}

void steering_Wandering( Vector2* pos, Vector2* vel, Vector2* currWanderTarget, float radius, float offset, float displacement, Vector2* newWanderTargetOut, Vector2* desiredVelocityOut )
{
	assert( pos != NULL );
	assert( vel != NULL );
	assert( desiredVelocityOut != NULL );
	assert( currWanderTarget != NULL );
	assert( newWanderTargetOut != NULL );
	assert( radius > 0.0f );

	// we'll use the circle method, we'll need a target that is stored as an offset from the current position

	// find circle center
	Vector2 circleCenter = ( *vel );
	vec2_Normalize( &circleCenter );
	vec2_Scale( &circleCenter, offset, &circleCenter );

	// create displaced target
	(*newWanderTargetOut) = vec2( rand_GetToleranceFloat( NULL, 0.0f, displacement ), rand_GetToleranceFloat( NULL, 0.0f, displacement ) );
	vec2_Add( currWanderTarget, newWanderTargetOut, newWanderTargetOut );

	// project displaced target onto circle
	Vector2 diff;
	vec2_Subtract( newWanderTargetOut, &circleCenter, &diff );
	vec2_Normalize( &diff );
	vec2_AddScaled( &circleCenter, &diff, radius, newWanderTargetOut );

	( *desiredVelocityOut ) = ( *newWanderTargetOut );
	vec2_Normalize( desiredVelocityOut );
}

//************************************

static void repositionTarget( void )
{
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );

	cam_ScreenPosToWorldPos( 0, &mousePos, &testTarget );
}

static int testSteeringScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	int width, height;
	gfx_GetRenderSize( &width, &height );
	cam_SetCenteredProjectionMatrix( 0, width, height );

	bordersMax.x = ( (float)width / 2.0f );
	bordersMax.y = ( (float)height / 2.0f );
	bordersMin.x = -( (float)width / 2.0f );
	bordersMin.y = -( (float)height / 2.0f );
	
	gfx_SetClearColor( CLR_BLACK );

	createVehicle( VEC2_ZERO, CLR_GREEN );

	input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, repositionTarget );

	return 1;
}

static int testSteeringScreen_Exit( void )
{
	return 1;
}

static void testSteeringScreen_ProcessEvents( SDL_Event* e )
{
}

static void testSteeringScreen_Process( void )
{
}

static void testSteeringScreen_Draw( void )
{
	drawVehicles( );

	debugRenderer_Circle( 1, testTarget, 5.0f, CLR_RED );
}

static void testSteeringScreen_PhysicsTick( float dt )
{
	/*Vector2 newVel;
	Vector2 newTarget;
	steering_Wandering( &( sbVehicles[0].pos ), &( sbVehicles[0].vel ), &( sbVehicles[0].wanderTarget ), 25.0f, 25.0f, 5.0f, &newTarget, &newVel );
	sbVehicles[0].wanderTarget = newTarget;//*/
	//steering_Seek( &( sbVehicles[0].pos ), &testTarget, &( sbVehicles[0].vel ) );
	steering_Flee( &( sbVehicles[0].pos ), &testTarget, &( sbVehicles[0].vel ) );
	//steering_Arrive( &( sbVehicles[0].pos ), &testTarget, 10.0f, 200.0f, &( sbVehicles[0].vel ) );

	vec2_Scale( &( sbVehicles[0].vel ), sbVehicles[0].maxSpeed, &( sbVehicles[0].vel ) );

	vehiclePhysics( dt );
}

GameState testSteeringScreenState = { testSteeringScreen_Enter, testSteeringScreen_Exit, testSteeringScreen_ProcessEvents,
	testSteeringScreen_Process, testSteeringScreen_Draw, testSteeringScreen_PhysicsTick };