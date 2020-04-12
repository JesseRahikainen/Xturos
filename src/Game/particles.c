#include "particles.h"
#include "Graphics/images.h"
#include "Graphics/color.h"
#include "Math/mathUtil.h"

#include "System/systems.h"

#include <string.h>

static int systemID = -1;

struct Particle {
	Vector2 currRenderPos;
	Vector2 futureRenderPos;
	Vector2 gravity;
	Vector2 velocity;
	float lifeTime;
	float lifeElapsed;
	float fadeStart;
	Color currColor;
	Color futureColor;
	float currScale;
	float futureScale;
	float rotRad;
	int image;
	char depth;
	unsigned int camFlags;
};

#define MAX_NUM_PARTICLES 512

int lastParticle;
static struct Particle particles[MAX_NUM_PARTICLES];

void physicsTick( float dt )
{
	int i;
	float fadeAmt;

	/* update the positions of all the particles */
	for( i = 0; i <= lastParticle; ++i ) {
		particles[i].lifeElapsed += dt;

		fadeAmt = inverseLerp( particles[i].fadeStart, particles[i].lifeTime, particles[i].lifeElapsed );
		particles[i].futureColor.a = lerp( 1.0f, 0.0f, fadeAmt );
		particles[i].futureScale = lerp( 1.0f, 0.0f, fadeAmt );

		particles[i].currRenderPos = particles[i].futureRenderPos;
		vec2_AddScaled( &particles[i].velocity, &particles[i].gravity, dt, &particles[i].velocity );
		vec2_AddScaled( &particles[i].futureRenderPos, &particles[i].velocity, dt, &particles[i].futureRenderPos );
	}

	/* destroy all the dead particles, we won't worry about preserving order but should do some tests to see
	    if doing so will make it more efficient */
	for( i = 0; i <= lastParticle; ++i ) {
		if( particles[i].lifeElapsed >= particles[i].lifeTime ) {
			particles[i] = particles[lastParticle];
			--lastParticle;
			--i;
		}
	}
}

void draw( void )
{
	for( int i = 0; i <= lastParticle; ++i ) {
		/*img_Draw_s( particles[i].image, particles[i].camFlags, particles[i].currRenderPos, particles[i].futureRenderPos,
			particles[i].currScale, particles[i].futureScale, particles[i].layer ); //*/
		img_Draw_s_r( particles[i].image, particles[i].camFlags, particles[i].currRenderPos, particles[i].futureRenderPos,
			particles[i].currScale, particles[i].futureScale, particles[i].rotRad, particles[i].rotRad, particles[i].depth );
		particles[i].currRenderPos = particles[i].futureRenderPos;
		particles[i].currColor = particles[i].futureColor;
		particles[i].currScale = particles[i].futureScale;
	}
}

int particles_Init( void )
{
	lastParticle = -1;

	systemID = sys_Register( NULL, NULL, draw, physicsTick );

	return systemID;
}

void particles_CleanUp( void )
{
	if( systemID >= 0 ) {
		sys_UnRegister( systemID );
		systemID = -1;
	}
}

void particles_Spawn( Vector2 startPos, Vector2 startVel, Vector2 gravity, float rotRad,
	float lifeTime, float fadeStart, int image, unsigned int camFlags, char layer )
{
	if( lastParticle >= ( MAX_NUM_PARTICLES - 1 ) ) {
		return;
	}

	++lastParticle;
	particles[lastParticle].currRenderPos = startPos;
	particles[lastParticle].futureRenderPos = startPos;
	particles[lastParticle].velocity = startVel;
	particles[lastParticle].gravity = gravity;
	particles[lastParticle].lifeTime = lifeTime;
	particles[lastParticle].fadeStart = MIN( fadeStart, lifeTime );
	particles[lastParticle].lifeElapsed = 0;
	particles[lastParticle].image = image;
	particles[lastParticle].depth = layer;
	particles[lastParticle].camFlags = camFlags;

	particles[lastParticle].currColor.r = particles[lastParticle].futureColor.r = 1.0f;
	particles[lastParticle].currColor.g = particles[lastParticle].futureColor.g = 1.0f;
	particles[lastParticle].currColor.b = particles[lastParticle].futureColor.b = 1.0f;
	particles[lastParticle].currColor.a = particles[lastParticle].futureColor.a = 1.0f;

	particles[lastParticle].currScale = particles[lastParticle].futureScale = 1.0f;

	particles[lastParticle].rotRad = rotRad;
}