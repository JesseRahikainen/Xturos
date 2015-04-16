#include "atlas.h"
#include <SDL_assert.h>
#include <SDL_opengl.h>

// experimenting with these, seeing how they work
#define STRETCHY_BUFFER_OUT_OF_MEMORY SDL_assert(0)
#include <stb_rect_pack.h>

#include "../System/memory.h"

#define free(ptr) mem_release(ptr)
#define realloc(ptr,size) mem_resize(ptr,size)
#define STB_STRETCH_BUFFER_IGNORE_STDLIB
#include <stretchy_buffer.h>
#undef STB_STRETCH_BUFFER_IGNORE_STDLIB
#undef free
#undef realloc

typedef struct {
	int inUse;
	GLuint textureID;

} Atlas;

Atlas* atlases = NULL;

static int findFirstUnusedAtlas( void )
{
	int size = sb_count( atlases );
	int idx = -1;

	for( int i = 0; ( i < size ) && ( idx == -1 ); ++i ) {
		if( !atlases[i].inUse ) {
			idx = i;
		}
	}

	if( idx == size ) {
		// reached end, add another
		//  note: this should double the size of the array each time
		sb_add( atlases, 1 );
	}

	return idx;
}

// resource management
// general algorithm will be quite simple, just sort by total size, find the largest that fits, add it and continue
//  but how do we deal with the packing failing, there isn't enough room left on the atlas? have some sort of id
//  that is based back (atlasImage) and it's -1 if it fails to pack that image
int createAtlas( int atlas, int numImages, void** imagesData, int* imagesWidths, int* imagesHeights, int* outAtlasImageIDs )
{
	int idx = findFirstUnusedAtlas( );
	if( idx < 0 ) {
		return -1;
	}

	


	return -1;
}

void destroyAtlas( int atlas )
{
	if( !atlases[atlas].inUse ) {
		return;
	}

	atlases[atlas].inUse = 0;
}

// rendering
void bindAtlasForRender( int atlas )
{

}