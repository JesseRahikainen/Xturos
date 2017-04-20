#ifndef ENGINE_PARTICLES_H
#define ENGINE_PARTICLES_H

#include "Math/vector2.h"

int particles_Init( void );
void particles_CleanUp( void );
void particles_Spawn( Vector2 startPos, Vector2 startVel, Vector2 gravity, float rotRad,
	float lifeTime, float fadeStart, int image, unsigned int camFlags, char layer );

#endif /* inclusion guard */
