#ifndef PERMUTATIONS_H
#define PERMUTATIONS_H

#include <stdbool.h>
#include <stdlib.h>

// for right now it will only take arrays of type int, can make it more general later
//  for all these values is an array of size n

// generates all permutations of values and sends each permutation to output
void heapsAlgorithm( int* values, int n, void ( *output )( int* v, int n ) );

// both of these assume you're starting with a sorted array
// these will take in an existing permutation and switch the array to the next permutation
//  that it should take
// SEPA (Simple Efficient Permutation Algorithm) will do the entire array
// SEPNKA (Simple Efficient PNK Algorithm) will generate the all permutations of size k (essentialy n permutation k)
bool SEPA( int* values, int n );
bool SEPNKA( int* values, int n, int k );

#endif