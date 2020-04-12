#ifndef HELPERS_H
#define HELPERS_H

// general helper functions
#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[0] ) )
#define ANY_BITS_ON( bits, testOnBits ) ( ( bits ) & ( testOnBits ) )
#define TURN_ON_BITS( currBits, onBits ) ( ( currBits ) |= ( onBits ) )
#define TURN_OFF_BITS( currBits, offBits ) ( ( currBits ) &= ~( offBits ) )

// copied from nuklear.h, don't want to have to rely on that
#if defined( _MSC_VER )
#define ALIGN_OF( t ) ( __alignof( t ) )
#else
// creates a structure with a byte and the desired type, casts 0 to it, then finds the difference between the position of the desired type and 0
#define ALIGN_OF( t ) ( (char*)( &( (struct { char c; t _h; }*)0 )->_h ) - (char*)0 )
#endif

void logMousePos( void );

#endif /* inclusion guard */