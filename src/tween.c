#include <assert.h>
#include <math.h>
#include "tween.h"
#include "Math/mathUtil.h"
#include <stdio.h>

static float pi = 3.14159265358979323846f;
static float twoPi = 6.283185307179586476925286766559f;

/* Sets the values for the tween. If no ease function is set it treats it as linear. */
void setTween( Tween* tween, float start, float end, float duration, float(*easeFunc)(float) )
{
	assert( tween );

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
	assert( tween );

	tween->current = tween->start;
	tween->elapsed = 0.0f;
	tween->active = 1;
}

/* Resets everything in the tween but swaps the start end end values. */
void resetAndReverseTween( Tween* tween )
{
	assert( tween );

	float temp = tween->start;
	tween->start = tween->end;
	tween->end = temp;

	resetTween( tween );
}

/* Processes the tween, advancing the amount elapsed. */
void processTween( Tween* tween, float delta )
{
	float t;
	assert( tween );

	tween->elapsed += delta;
	if( tween->elapsed > tween->duration ) {
		tween->elapsed = tween->duration;
		tween->active = 0;
	}

	t = tween->elapsed / tween->duration;
	if( tween->ease ) {
		t = tween->ease( t );
	}
	tween->current = lerp( tween->start, tween->end, t );
}

/* Some general tween functions. */
float easeInSin( float t )
{
	return ( -cosf( twoPi * t ) + 1.0f );
}

float easeOutSin( float t )
{
	return sinf( twoPi * t );
}

float easeInOutSin( float t )
{
	return ( ( -cosf( pi * t ) / 2.0f ) + 0.5f );
}

float easeInQuad( float t )
{
	return ( t * t );
}

float easeOutQuad( float t )
{
	return ( -t * ( t - 2.0f ) );
}

float easeInOutQuad( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * 2.0f );
	} else {
		t -= 1.0f;
		return ( 1.0f - ( t * t * 2.0f ) );
	}
}

float easeInCubic( float t )
{
	return ( t * t * t );
}

float easeOutCubic( float t )
{
	t -= 1.0f;
	return ( 1.0f + ( t * t * t ) );
}

float easeInOutCubic( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * t * 4.0f );
	} else {
		t -= 1.0f;
		return ( 1.0f + ( t * t * t * 4.0f ) );
	}
}

float easeInQuart( float t )
{
	return ( t * t * t * t );
}

float easeOutQuart( float t )
{
	t -= 1.0f;
	return ( 1.0f - ( t * t * t * t ) );
}

float easeInOutQuart( float t )
{
	if( t <= 0.5f ) {
		return ( t * t * t * t * 8.0f );
	} else {
		t = ( t * 2.0f ) - 2.0f;
		return ( ( ( 1.0f - ( t * t * t * t ) ) / 2.0f ) + 0.5f );
	}
}

float easeInQuint( float t )
{
	return t * t * t * t * t;
}

float easeOutQuint( float t )
{
	t -= 1.0f;
	return ( ( t * t * t * t * t ) + 1.0f );
}

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

float easeInExpo( float t )
{
	return powf( 2.0f, 10.0f * ( t - 1.0f ) );
}

float easeOutExpo( float t )
{
	return ( -powf( 2.0f, ( -10.0f * t ) ) + 1.0f );
}

float easeInOutExpo( float t )
{
	if( t < 0.5f ) {
		return powf( 2.0f, ( 10.0f * ( ( t * 2.0f ) - 1.0f ) ) ) / 2.0f;
	} else {
		return ( -powf( 2.0f, -10.0f * ( ( t * 2.0f ) - 1.0f ) ) + 2.0f ) / 2.0f;
	}
}

float easeInCirc( float t )
{
	return -( sqrtf( 1.0f - ( t * t ) - 1.0f ) );
}

float easeOutCirc( float t )
{
	t -= 1.0f;
	return sqrtf( 1.0f - ( t * t ) );
}

float easeInOutCirc( float t )
{
	if( t <= 0.5f ) {
		return ( ( sqrtf( 1.0f - ( t * t * 4.0f ) ) - 1.0f ) / -2.0f );
	} else {
		t = ( ( t * 2.0f ) - 2.0f );
		return ( ( sqrtf( 1.0f - ( t * t ) ) + 1.0f ) / 2.0f );
	}
}

float easeInBack( float t )
{
	return ( t * t * ( ( 2.70158f * t ) - 1.70158f ) );
}

float easeOutBack( float t )
{
	t -= 1.0f;
	return ( 1 - ( t * t * ( ( -2.70158f * t ) - 1.70158f ) ) );
}

float easeInOutBack( float t )
{
	t *= 2.0f;
	if( t < 1.0f ) {
		return ( t * t * ( ( ( 2.70158f * t ) - 1.70158f ) / 2.0f ) );
	}
	t -= 2.0f;
	return ( 1.0f - ( t * t * ( ( -2.70158f * t ) - 1.70158f ) / 2.0f ) + 0.5f );
}

float easeInBounce( float t )
{
	return easeOutBounce( 1.0f - t );
}

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

float easeInOutBounce( float t )
{
	if( t < 0.5f ) {
		return easeInBounce( t * 2 ) * 0.5f;
	} else {
		return ( easeOutBounce( ( t * 2.0f ) - 1.0f ) * 0.5f ) + 0.5f;
	}
}

float easeConstantZero( float t )
{
	return 0.0f;
}

float easeConstantOne( float t )
{
	return 1.0f;
}

float easeLinear( float t )
{
	return t;
}