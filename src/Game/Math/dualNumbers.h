#ifndef DUAL_NUMBERS_H
#define DUAL_NUMBERS_H

// http://www.dtecta.com/files/GDC13_vandenBergen_Gino_Math_Tut.pdf
// https://github.com/dtecta/motion-toolkit/blob/master/moto/Dual.hpp

// TODO: Use macros to make a templated version of this so we can use it with anything.

typedef struct {
	float real;
	float dual;
} DualNumber;

DualNumber dual( float r, float d );

DualNumber* dual_Add( const DualNumber* lhs, const DualNumber* rhs, DualNumber* out );
DualNumber* dual_Subtract( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out );
DualNumber* dual_Multiply( const  DualNumber* lhs, const DualNumber* rhs, DualNumber* out );
DualNumber* dual_Divide( const DualNumber* lhs, const DualNumber* rhs, DualNumber* out );

DualNumber* dual_Negate( const DualNumber* in, DualNumber* out );

// any differentiable function can be extended to dual numbers as f(a + be) = f(a) + b*f'(a)e
//  some example ones below, extend as needed
DualNumber* dual_Sin( const DualNumber* in, DualNumber* out );
DualNumber* dual_Cos( const DualNumber* in, DualNumber* out );
DualNumber* dual_Tan( const DualNumber* in, DualNumber* out );

int dual_Compare( const DualNumber* lhs, const DualNumber* rhs );

#endif
