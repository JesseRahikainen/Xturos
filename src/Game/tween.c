#include <SDL_assert.h>
#include <SDL_stdinc.h>
#include "tween.h"
#include "Math/mathUtil.h"
#include <stdio.h>

#include "System/ECPS/ecps_trackedCallbacks.h"

static float pi = 3.14159265358979323846f;
static float twoPi = 6.283185307179586476925286766559f;

/* Sets the values for the tween. If no ease function is set it treats it as linear. */
void setTween( Tween* tween, float start, float end, float duration, float(*easeFunc)(float) )
{
	SDL_assert( tween );

	tween->start = start;
	tween->end = end;
	tween->duration = duration;
	tween->ease = easeFunc;

	tween->current = start;
	tween->elapsed = 0.0f;
	tween->active = 1;
}

/* Resets everything for the tween. */
void resetTween( Tween* tween )
{
	SDL_assert( tween );

	tween->current = tween->start;
	tween->elapsed = 0.0f;
	tween->active = 1;
}

/* Resets everything in the tween but swaps the start end end values. */
void resetAndReverseTween( Tween* tween )
{
	SDL_assert( tween );

	float temp = tween->start;
	tween->start = tween->end;
	tween->end = temp;

	resetTween( tween );
}

/* Processes the tween, advancing the amount elapsed. */
bool processTween( Tween* tween, float delta )
{
	float t;
	SDL_assert( tween );

	bool isDone = false;

	tween->elapsed += delta;
	if( tween->elapsed > tween->duration ) {
		isDone = true;
		tween->elapsed = tween->duration;
		tween->active = 0;
	}

	t = tween->elapsed / tween->duration;
	if( tween->ease ) {
		t = tween->ease( t );
	}
	tween->current = lerp( tween->start, tween->end, t );

	return isDone;
}

/* Some general tween functions. */
float easeInSin( float t )
{
	return ( -SDL_cosf( twoPi * t ) + 1.0f );
}
ADD_TRACKED_TWEEN_FUNC( "easeInSin", easeInSin );

float easeOutSin( float t )
{
	return SDL_sinf( twoPi * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutSin", easeOutSin );

float easeInOutSin( float t )
{
	return ( ( -SDL_cosf( pi * t ) / 2.0f ) + 0.5f );
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutSin", easeInOutSin );

float easeInQuad( float t )
{
	return ( t * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeInQuad", easeInQuad );

float easeOutQuad( float t )
{
	return ( -t * ( t - 2.0f ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutQuad", easeOutQuad );

float easeInOutQuad( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * 2.0f );
	} else {
		t -= 1.0f;
		return ( 1.0f - ( t * t * 2.0f ) );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutQuad", easeInOutQuad );

float easeInCubic( float t )
{
	return ( t * t * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeInCubic", easeInCubic );

float easeOutCubic( float t )
{
	t -= 1.0f;
	return ( 1.0f + ( t * t * t ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutCubic", easeOutCubic );

float easeInOutCubic( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * t * 4.0f );
	} else {
		t -= 1.0f;
		return ( 1.0f + ( t * t * t * 4.0f ) );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutCubic", easeInOutCubic );

float easeInQuart( float t )
{
	return ( t * t * t * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeInQuart", easeInQuart );

float easeOutQuart( float t )
{
	t -= 1.0f;
	return ( 1.0f - ( t * t * t * t ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutQuart", easeOutQuart );

float easeInOutQuart( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * t * t * 8.0f );
	} else {
		t = ( t * 2.0f ) - 2.0f;
		return ( ( ( 1.0f - ( t * t * t * t ) ) / 2.0f ) + 0.5f );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutQuart", easeInOutQuart );

float easeInQuint( float t )
{
	return t * t * t * t * t;
}
ADD_TRACKED_TWEEN_FUNC( "easeInQuint", easeInQuint );

float easeOutQuint( float t )
{
	t -= 1.0f;
	return ( ( t * t * t * t * t ) + 1.0f );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutQuint", easeOutQuint );

float easeInOutQuint( float t )
{
	t *= 2.0f;
	if( t < 1 ) {
		return ( ( t * t * t * t * t ) / 2.0f );
	} else {
		t -= 2.0f;
		return ( ( ( t * t * t * t * t ) + 2.0f ) / 2.0f );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutQuint", easeInOutQuint );

float easeInExpo( float t )
{
	return SDL_powf( 2.0f, 10.0f * ( t - 1.0f ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeInExpo", easeInExpo );

float easeOutExpo( float t )
{
	return ( -SDL_powf( 2.0f, ( -10.0f * t ) ) + 1.0f );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutExpo", easeOutExpo );

float easeInOutExpo( float t )
{
	if( t < 0.5f ) {
		return SDL_powf( 2.0f, ( 10.0f * ( ( t * 2.0f ) - 1.0f ) ) ) / 2.0f;
	} else {
		return ( -SDL_powf( 2.0f, -10.0f * ( ( t * 2.0f ) - 1.0f ) ) + 2.0f ) / 2.0f;
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutExpo", easeInOutExpo );

float easeInCirc( float t )
{
	return -( SDL_sqrtf( 1.0f - ( t * t ) ) - 1.0f );
}
ADD_TRACKED_TWEEN_FUNC( "easeInCirc", easeInCirc );

float easeOutCirc( float t )
{
	t -= 1.0f;
	return SDL_sqrtf( 1.0f - ( t * t ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutCirc", easeOutCirc );

float easeInOutCirc( float t )
{
	if( t <= 0.5f ) {
		return ( ( SDL_sqrtf( 1.0f - ( t * t * 4.0f ) ) - 1.0f ) / -2.0f );
	} else {
		t = ( ( t * 2.0f ) - 2.0f );
		return ( ( SDL_sqrtf( 1.0f - ( t * t ) ) + 1.0f ) / 2.0f );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutCirc", easeInOutCirc );

float easeInBack( float t )
{
	return ( t * t * ( ( 2.70158f * t ) - 1.70158f ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeInBack", easeInBack );

float easeOutBack( float t )
{
	t -= 1.0f;
	return ( 1 - ( t * t * ( ( -2.70158f * t ) - 1.70158f ) ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeOutBack", easeOutBack );

float easeInOutBack( float t )
{
	t *= 2.0f;
	if( t < 1.0f ) {
		return ( t * t * ( ( ( 2.70158f * t ) - 1.70158f ) / 2.0f ) );
	}
	t -= 2.0f;
	return ( 1.0f - ( t * t * ( ( -2.70158f * t ) - 1.70158f ) / 2.0f ) + 0.5f );
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutBack", easeInOutBack );

float easeInBounce( float t )
{
	return easeOutBounce( 1.0f - t );
}
ADD_TRACKED_TWEEN_FUNC( "easeInBounce", easeInBounce );

float easeOutBounce( float t )
{
	// copied from https://github.com/warrenm/AHEasing/blob/master/AHEasing/easing.c
	if( t < ( 4.0f / 11.0f ) ) {
		return ( 121.0f * t * t ) / 16.0f;
	} else if( t < ( 8.0f / 11.0f ) ) {
		return ( 363.0f / 40.0f * t * t ) - ( 99.0f / 10.0f * t ) + ( 17.0f / 5.0f );
	} else if( t < ( 9.0f / 10.0f ) ) {
		return ( 4356.0f / 361.0f * t * t ) - ( 35442.0f / 1805.0f * t ) + ( 16061.0f / 1805.0f );
	} else {
		return ( 54.0f / 5.0f * t * t ) - ( 513.0f / 25.0f * t ) + ( 268.0f / 25.0f );
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeOutBounce", easeOutBounce );

float easeInOutBounce( float t )
{
	if( t < 0.5f ) {
		return easeInBounce( t * 2 ) * 0.5f;
	} else {
		return ( easeOutBounce( ( t * 2.0f ) - 1.0f ) * 0.5f ) + 0.5f;
	}
}
ADD_TRACKED_TWEEN_FUNC( "easeInOutBounce", easeInOutBounce );

float easeConstantZero( float t )
{
	return 0.0f;
}
ADD_TRACKED_TWEEN_FUNC( "easeConstantZero", easeConstantZero );

float easeConstantOne( float t )
{
	return 1.0f;
}
ADD_TRACKED_TWEEN_FUNC( "easeConstantOne", easeConstantOne );

float easeLinear( float t )
{
	return t;
}
ADD_TRACKED_TWEEN_FUNC( "easeLinear", easeLinear );

float easeSmoothStep( float t )
{
	return ( -2.0f * ( t * t * t ) ) + ( 3.0f * ( t * t ) );
}
ADD_TRACKED_TWEEN_FUNC( "easeSmoothStep", easeSmoothStep );

float easePerlinQuintic( float t )
{
	return ( 6.0f * ( t * t * t * t * t ) ) + ( -15.0f * ( t * t * t * t ) ) + ( 10.0f * ( t * t * t ) );
}
ADD_TRACKED_TWEEN_FUNC( "easePerlinQuintic", easePerlinQuintic );

float easeSlowMiddle( float t )
{
	return ( 3.2f * ( t * t * t ) ) + ( 4.8f * ( t * t ) ) + ( 2.6f * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeSlowMiddle", easeSlowMiddle );

float easeFullSinWave( float t )
{
	return ( SDL_sinf( t * M_TWO_PI_F ) + 1.0f ) / 2.0f;
}
ADD_TRACKED_TWEEN_FUNC( "easeFullSinWave", easeFullSinWave );

// goes from 0, up to 1, then back down to 0
float easeArc( float t )
{
	return ( -4.0f * ( t * t ) ) + ( 4.0f * t );
}
ADD_TRACKED_TWEEN_FUNC( "easeArc", easeArc );