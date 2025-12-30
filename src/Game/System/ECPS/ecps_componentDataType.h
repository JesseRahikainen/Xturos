#ifndef COMPONENT_DATA_TYPE
#define COMPONENT_DATA_TYPE

#include "ecps_dataTypes.h"

#include "System/serializer.h"

// component callbacks
typedef int ( *VerifyComponent )( EntityID entityID );
typedef void ( *CleanUpComponent )( ECPS* ecps, const Entity* entity, void* compData, bool fullCleanUp ); // fullCleanUp is true when every entity is being destroyed
typedef bool ( *SerializeComponent )( Serializer* s, void* data );

struct ComponentType {
	char name[MAX_COMPONENT_NAME_SIZE + 1];
	uint32_t version;
	size_t size;
	size_t align; // desired aligment of the structure

	// this was added to make sure the values of a component are correct (e.g. the character doesn't move off the screen)
	//  more of a debugging tool than something you should use in a game, think of it like an assert( )
	VerifyComponent verify;

	// used when there is clean up that needs to be done after a component is removed or
	//  an entity containing it is destroyed
	CleanUpComponent cleanUp;

	// serialization functions, used to save out the components, if you don't need to worry about serializing or deserializing stuff you can ignore it
	SerializeComponent serialize;
};

#endif // inclusion guard