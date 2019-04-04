#include "testSteeringScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../Utils/stretchyBuffer.h"

#include <math.h>
#include <assert.h>

typedef struct {
	Vector2 linearAccel;
	float angularAccelRad;
} SteeringOutput;

typedef struct {
	Vector2 pos;
	Vector2 vel;
	float maxSpeed;

	float rotRad;
	float rotSpdRad;
	float maxRotSpeedRad;

	SteeringOutput steering;

	float radius;
	Color clr;
} SteeringVehicle;

SteeringVehicle* sbVehicles = NULL;

static void createVehicle( Vector2 pos, Color clr )
{
	SteeringVehicle v;

	v.pos = pos;
	v.vel = VEC2_ZERO;
	v.maxSpeed = 100.0f;

	v.rotRad = 0.0f;
	v.rotSpdRad = 0.0f;
	v.maxRotSpeedRad = DEG_TO_RAD( 360.0f );

	v.steering.linearAccel = VEC2_ZERO;
	v.steering.angularAccelRad = 0.0f;

	v.radius = 10.0f;
	v.clr = clr;

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
		vec2_AddScaled( &( sbVehicles[i].pos ), &( sbVehicles[i].steering.linearAccel ), 0.5f * dt * dt, &( sbVehicles[i].pos ) );

		sbVehicles[i].rotRad += ( sbVehicles[i].rotSpdRad * dt ) + ( 0.5f * dt * dt * sbVehicles[i].steering.angularAccelRad );

		vec2_AddScaled( &( sbVehicles[i].vel ), &( sbVehicles[i].steering.linearAccel ), dt, &( sbVehicles[i].vel ) );
		if( vec2_MagSqrd( &(sbVehicles[i].vel) ) > ( sbVehicles[i].maxSpeed * sbVehicles[i].maxSpeed ) ) {
			vec2_Normalize( &(sbVehicles[i].vel) );
			vec2_Scale( &(sbVehicles[i].vel), sbVehicles[i].maxSpeed, &(sbVehicles[i].vel) );
		}

		sbVehicles[i].rotSpdRad += sbVehicles[i].steering.angularAccelRad * dt;
		if( fabsf( sbVehicles[i].rotSpdRad ) > sbVehicles[i].maxRotSpeedRad ) {
			sbVehicles[i].rotSpdRad = sbVehicles[i].maxRotSpeedRad * sign( sbVehicles[i].rotSpdRad );
		}
	}
}

//************************************
// Basic Steering Behaviors

/*void steering_Seek( SteeringVehicle* vehicle, Vector2* target, Vector2* out )
{
	assert( vehicle != NULL );
	assert( target != NULL );
	assert( out != NULL );

	Vector2 diff;
	vec2_Subtract( &(vehicle->pos), target, &diff );
	vec2_Normalize( &diff );
	vec2_Scale( &diff, vehicle->maxSpeed, &diff );
	
	vec2_Subtract( &(
}

void steering_Flee( Vector2* pos, Vector2* target, Vector2* out )
{

}

void steering_Arrive( //*/

//************************************

//************************************
// Steering Behavior Combining

//************************************


static int testSteeringScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	int width, height;
	gfx_GetRenderSize( &width, &height );
	cam_SetCenteredProjectionMatrix( 0, width, height );
	
	gfx_SetClearColor( CLR_BLACK );

	createVehicle( VEC2_ZERO, CLR_GREEN );

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
}

static void testSteeringScreen_PhysicsTick( float dt )
{
	vehiclePhysics( dt );
}

struct GameState testSteeringScreenState = { testSteeringScreen_Enter, testSteeringScreen_Exit, testSteeringScreen_ProcessEvents,
	testSteeringScreen_Process, testSteeringScreen_Draw, testSteeringScreen_PhysicsTick };