#ifndef HELPERS_H
#define HELPERS_H

// general helper functions
#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[0] ) )
#define ANY_BITS_ON( bits, testOnBits ) ( ( flags) & ( testBits ) )
#define TURN_ON_BITS( currBits, onBits ) ( ( currBits ) |= ( onBits ) )
#define TURN_OFF_BITS( currBits, offBits ) ( ( currBits ) &= ~( offBits ) )

#endif /* inclusion guard */