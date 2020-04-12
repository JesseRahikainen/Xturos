/*
Simple handling of lerping between two values.
*/

#ifndef ENGINE_TWEEN_H
#define ENGINE_TWEEN_H

typedef float (*EaseFunc)(float);

typedef struct {
	int active;
	float current;

	float start;
	float end;
	float duration;
	float elapsed;

	EaseFunc ease;
} Tween;


/* Sets the values for the tween. If no ease function is set it treats it as linear. */
void setTween( Tween* tween, float start, float end, float duration, EaseFunc easeFunc );

/* Resets everything for the tween. */
void resetTween( Tween* tween );

/* Resets everything in the tween but swaps the start end end values. */
void resetAndReverseTween( Tween* tween );

/* Processes the tween, advancing the amount elapsed. */
void processTween( Tween* tween, float delta );

/* Some general tween functions. */
float easeInSin( float t );
float easeOutSin( float t );
float easeInOutSin( float t );
float easeInQuad( float t );
float easeOutQuad( float t );
float easeInOutQuad( float t );
float easeInCubic( float t );
float easeOutCubic( float t );
float easeInOutCubic( float t );
float easeInQuart( float t );
float easeOutQuart( float t );
float easeInOutQuart( float t );
float easeInQuint( float t );
float easeOutQuint( float t );
float easeInOutQuint( float t );
float easeInExpo( float t );
float easeOutExpo( float t );
float easeInOutExpo( float t );
float easeInCirc( float t );
float easeOutCirc( float t );
float easeInOutCirc( float t );
float easeInBack( float t );
float easeOutBack( float t );
float easeInOutBack( float t );
float easeInBounce( float t );
float easeOutBounce( float t );
float easeInOutBounce( float t );
float easeConstantZero( float t );
float easeConstantOne( float t );
float easeLinear( float t );

#endif
