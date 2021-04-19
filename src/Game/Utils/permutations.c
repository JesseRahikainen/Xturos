#include "permutations.h"

#include "../System/memory.h"

static void swap( int* values, int n0, int n1 )
{
	int temp = values[n0];
	values[n0] = values[n1];
	values[n1] = temp;
}

static void reverseRightOf( int n, int* values, int rightOf )
{
	int left = rightOf;
	int right = n - 1;
	while( right > left ) {
		swap( values, left, right );
		++left;
		--right;
	}
}

// non-recursive heap's algorithm
void heapsAlgorithm( int* values, int n, void ( *output )( int* v, int c ) )
{
	int* c = mem_Allocate( sizeof( int ) * n );

	for( int i = 0; i < n; ++i ) {
		c[i] = 0;
	}

	output( values, n );

	int i = 0;
	while( i < n ) {
		if( c[i] < i ) {
			if( ( i % 2 ) == 0 ) {
				swap( values, 0, i );
			} else {
				swap( values, c[i], i );
			}
			output( values, n );

			c[i] += 1;
			i = 0;
		} else {
			c[i] = 0;
			i += 1;
		}
	}

	mem_Release( c );
}

// https://quickperm.org/soda_submit.php
bool SEPA( int* values, int n )
{
	int key = n - 1;
	int newKey = n - 1;

	// search for the right most ascent (where the left number is less than the right number)
	while( ( key > 0 ) && ( values[key] <= values[key - 1] ) ) {
		--key;
	}
	--key;

	// if the key is < 0 then the values are in reverse sorted order and there is none left
	if( key < 0 ) return false;

	// so now we have |head|key|tail|

	// the key must be swapped with the smallest value in the tail which is larger than the key value
	//  we know that at least the first value of the tail has to be greater than the key, otherwise
	//  it wouldn't be an ascent
	while( ( newKey > key ) && ( values[newKey] <= values[key] ) ) {
		--newKey;
	}

	// swap
	swap( values, key, newKey );

	// the tail must be in sorted order to produce the next permutation
	reverseRightOf( n, values, key + 1 );

	return true;
}

// https://alistairisrael.wordpress.com/2009/09/22/simple-efficient-pnk-algorithm/
// assume we're starting with a sorted array
// returns if it's the last k permutation of n
// was based on the SEPA algorithm above
bool SEPNKA( int* values, int n, int k )
{
	// we only care about elements at the edge or to the left
	int edge = k - 1;

	// start out assuming the edge contains our ascent, look to right for the next higher number
	int j = k;
	while( ( j < n ) && ( values[edge] >= values[j] ) ) {
		++j;
	}

	if( j < n ) {
		// assumption was correct, swap the edge with the found value
		swap( values, edge, j );
	} else {
		// assumption was wrong, ascent is to the left of the edge

		// all numbers to the right of the edge are in ascending order, which would violate the
		//  SEPA algorithm, so reverse them to get them into descending order
		reverseRightOf( n, values, k );

		// find the right most ascent
		int i = edge - 1;
		while( ( i >= 0 ) && ( values[i] >= values[i + 1] ) ) {
			--i;
		}

		// no more premutations
		if( i < 0 ) {
			return false;
		}

		// find the value to swap with
		j = n - 1;
		while( ( j > i ) && ( values[i] >= values[j] ) ) {
			--j;
		}

		swap( values, i, j );

		// then reverse again, like in SEPA the tail must be in sorted order and we know it's in reverse
		reverseRightOf( n, values, i + 1 );
	}

	return true;
}