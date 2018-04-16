#ifndef CA_VALUES_H
#define CA_VALUES_H

#define MAX_NUM_COMPONENT_TYPES ( 30 + 2 ) // should always be in the range [1, UINT32_MAX - 1], extra 2 is for the id and enabled flag
#if ( MAX_NUM_COMPONENT_TYPES <= 0 )
	#error "MAX_NUM_COMPONENT_TYPES must greater than 0"
#endif
#if ( MAX_NUM_COMPONENT_TYPES > UINT32_MAX )
	#error "MAX_NUM_COMPONENT_TYPES must be less than or equal to UINT32_MAX"
#endif
#define FLAGS_ARRAY_SIZE ( ( MAX_NUM_COMPONENT_TYPES + 31 ) / 32 )

#endif