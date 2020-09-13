#include "images.h"

#include "../Graphics/glPlatform.h"
#include "glPlatform.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "../Math/matrix4.h"
#include "gfxUtil.h"
#include "../System/platformLog.h"
#include "../Math/mathUtil.h"

#include "../System/jobQueue.h"
#include "../System/jobRingQueue.h"
#include "../System/memory.h"

#include "../Utils/hashMap.h"

/* Image loading types and variables */
#define MAX_IMAGES 512

enum {
	IMGFLAG_IN_USE = 0x1,
	IMGFLAG_HAS_TRANSPARENCY = 0x2,
};

typedef struct {
	GLuint textureObj;
	Vector2 uvMin;
	Vector2 uvMax;
	Vector2 size;
	Vector2 offset;
	int flags;
	int packageID;
	int nextInPackage;
	ShaderType shaderType;
} Image;

static Image images[MAX_IMAGES];

/* Rendering types and variables */
#define MAX_RENDER_INSTRUCTIONS 1024
typedef struct {
	Vector2 pos;
	Vector2 scaledSize;
	Color color;
	float rotation;
	Vector2 offset;
	float floatVal0;
} DrawInstructionState;

typedef struct {
	GLuint textureObj;
	int imageObj;
	Vector2 uvs[4];
	DrawInstructionState start;
	DrawInstructionState end;
	int flags;
	int stencilID;
	uint32_t camFlags;
	int8_t depth;
	ShaderType shaderType;
	bool isStencil;
} DrawInstruction;

static DrawInstruction renderBuffer[MAX_RENDER_INSTRUCTIONS];
static int lastDrawInstruction;

static const DrawInstruction DEFAULT_DRAW_INSTRUCTION = {
	0, -1,
	{ { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f, { 0.0f, 0.0f } },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f, { 0.0f, 0.0f } },
	0, 0, 0
};

static GLint maxTextureSize;

static HashMap imgIDMap;

/*
Initializes images.
 Returns < 0 on an error.
*/
int img_Init( void )
{
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );
	memset( images, 0, sizeof(images) );
	hashMap_Init( &imgIDMap, 64, NULL );
	return 0;
}

/*
Finds the first unused image index.
 Returns a postive value on success, a negative on failure.
*/
static int findAvailableImageIndex( )
{
	int newIdx = 0;

	while( ( newIdx < MAX_IMAGES ) && ( images[newIdx].flags & IMGFLAG_IN_USE ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_IMAGES ) {
		newIdx = -1;
	}

	return newIdx;
}

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int img_Load( const char* fileName, ShaderType shaderType )
{
	int newIdx = -1;

	// if we've already loaded the image don't load it again
	if( hashMap_Find( &imgIDMap, fileName, &newIdx ) ) {
		return newIdx;
	}

	// find the first empty spot, make sure we won't go over our maximum
	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to load image %s! Image storage full.", fileName );
		return -1;
	}

	Texture texture;
	if( gfxUtil_LoadTexture( fileName, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to load image %s!", fileName );
		newIdx = -1;
		return -1;
	}

	images[newIdx].textureObj = texture.textureID;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	hashMap_Set( &imgIDMap, fileName, newIdx );

	return newIdx;
}

int img_CreateFromLoadedImage( LoadedImage* loadedImg, ShaderType shaderType, const char* id )
{
	int newIdx = -1;

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image! Image storage full." );
		return -1;
	}

	Texture texture;
	if( gfxUtil_CreateTextureFromLoadedImage( GL_RGBA, loadedImg, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to create image!" );
		return -1;
	}

	images[newIdx].textureObj = texture.textureID;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	if( id != NULL ) {
		hashMap_Set( &imgIDMap, id, newIdx );
	}

	return newIdx;
}

int img_CreateFromTexture( Texture* texture, ShaderType shaderType, const char* id )
{
	int newIdx = -1;;

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image! Image storage full." );
		return -1;
	}

	images[newIdx].textureObj = texture->textureID;
	images[newIdx].size.v[0] = (float)texture->width;
	images[newIdx].size.v[1] = (float)texture->height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	if( texture->flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	if( id != NULL ) {
		hashMap_Set( &imgIDMap, id, newIdx );
	}

	return newIdx;
}

typedef struct {
	char* fileName;
	ShaderType shaderType;
	int* outIdx;
	LoadedImage loadedImage;
} ThreadedLoadImageData;

static void bindImageJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_INFO, "No data, unable to bind." );
		return;
	}

	ThreadedLoadImageData* loadData = (ThreadedLoadImageData*)data;

	(*(loadData->outIdx)) = -1;

	if( loadData->loadedImage.data == NULL ) {
		llog( LOG_INFO, "Failed to load image %s", loadData->fileName );
		goto clean_up;
	}

	// find the first empty spot, make sure we won't go over our maximum
	int newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to bind image %s! Image storage full.", loadData->fileName );
		goto clean_up;
	}

	Texture texture;
	if( gfxUtil_CreateTextureFromLoadedImage( GL_RGBA, &( loadData->loadedImage ), &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to bind image %s!", loadData->fileName );
		goto clean_up;
	}

	images[newIdx].textureObj = texture.textureID;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = loadData->shaderType;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
	(*(loadData->outIdx)) = newIdx;

	llog( LOG_INFO, "Setting outIdx to %i", newIdx );

clean_up:
	gfxUtil_ReleaseLoadedImage( &( loadData->loadedImage ) );
	mem_Release( loadData->fileName );
	mem_Release( loadData );
}

static void loadImageJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_INFO, "No data, unable to load." );
		return;
	}

	ThreadedLoadImageData* loadData = (ThreadedLoadImageData*)data;
	// will worry about handling a bad load when we get back to the main thread
	gfxUtil_LoadImage( loadData->fileName, &( loadData->loadedImage ) );

	// binding needs to be done on the main thread
	jq_AddMainThreadJob( bindImageJob, data );
}

/*
Loads the image in a seperate thread. Puts the resulting image index into outIdx.
 Returns -1 if there was an issue, 0 otherwise.
*/
void img_ThreadedLoad( const char* fileName, ShaderType shaderType, int* outIdx )
{
	assert( fileName != NULL );

	// set it to something that won't draw anything
	(*outIdx) = -1;

	// this isn't something that should be happening all the time, so allocating and freeing
	//  should be fine, if we want to do continous streaming it would probably be better to
	//  make a specialty queue for it
	ThreadedLoadImageData* data = mem_Allocate( sizeof( ThreadedLoadImageData ) );
	if( data == NULL ) {
		llog( LOG_WARN, "Unable to create data for threaded image load for file %s", fileName );
		return;
	}

	size_t fileNameLen = strlen( fileName );
	data->fileName = mem_Allocate( fileNameLen + 1 );
	if( data->fileName == NULL ) {
		llog( LOG_WARN, "Unable to create file name storage for threaded image lead for fle %s", fileName );
		mem_Release( data );
		return;
	}
	SDL_strlcpy( data->fileName, fileName, fileNameLen );

	data->shaderType = shaderType;
	data->outIdx = outIdx;
	data->loadedImage.data = NULL;

	if( !jq_AddJob( loadImageJob, data ) ) {
		mem_Release( data );
	}
}

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface, ShaderType shaderType, const char* id )
{
	int newIdx;

	assert( surface != NULL );

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image from surface! Image storage full." );
		return -1;
	}

	Texture texture;
	if( gfxUtil_CreateTextureFromSurface( surface, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to convert surface to texture! SDL Error: %s", SDL_GetError( ) );
		return -1;
	} else {
		images[newIdx].size.v[0] = (float)texture.width;
		images[newIdx].size.v[1] = (float)texture.height;
		images[newIdx].offset = VEC2_ZERO;
		images[newIdx].packageID = -1;
		images[newIdx].flags = IMGFLAG_IN_USE;
		images[newIdx].nextInPackage = -1;
		images[newIdx].uvMin = VEC2_ZERO;
		images[newIdx].uvMax = VEC2_ONE;
		images[newIdx].shaderType = shaderType;
		if( texture.flags & TF_IS_TRANSPARENT ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}
	}

	if( id != NULL ) {
		hashMap_Set( &imgIDMap, id, newIdx );
	}

	return newIdx;
}

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void img_Clean( int idx )
{
	assert( idx < MAX_IMAGES );
	assert( idx >= 0 );
	assert( images[idx].flags & IMGFLAG_IN_USE );

	int bufIdx;

	if( ( idx < 0 ) || ( ( images[idx].size.v[0] == 0.0f ) && ( images[idx].size.v[1] == 0.0f ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	/* clean up anything we're wanting to draw */
	for( bufIdx = 0; bufIdx <= lastDrawInstruction; ++bufIdx ) {
		if( renderBuffer[bufIdx].imageObj == idx ) {
			if( lastDrawInstruction > 0 ) {
				renderBuffer[bufIdx] = renderBuffer[lastDrawInstruction-1];
			}
			--lastDrawInstruction;
			--bufIdx;
		}
	}

	// see if this is the last image using that texture
	//  TODO: See if this needs to be sped up
	int deleteTexture = 1;
	for( int i = 0; ( i < MAX_IMAGES ) && deleteTexture; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].textureObj == images[idx].textureObj ) ) {
			deleteTexture = 0;
		}
	}

	if( deleteTexture ) {
		glDeleteTextures( 1, &( images[idx].textureObj ) );
	}
	images[idx].size = VEC2_ZERO;
	images[idx].flags = 0;
	images[idx].packageID = -1;
	images[idx].nextInPackage = -1;
	images[idx].uvMin = VEC2_ZERO;
	images[idx].uvMax = VEC2_ZERO;
	images[idx].shaderType = ST_DEFAULT;

	hashMap_RemoveFirstByValue( &imgIDMap, idx );
}

/*
Finds the next unused package ID.
*/
int findUnusedPackage( void )
{
	int packageID = 0;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID <= packageID ) ) {
			packageID = images[i].packageID + 1;
		}
	}
	return packageID;
}

/*
Splits the texture. Returns a negative number if there's a problem.
*/
int split( Texture* texture, int packageID, ShaderType shaderType, int count, Vector2* mins, Vector2* maxes, char** imgIDs, int* retIDs )
{
	Vector2 inverseSize;
	inverseSize.x = 1.0f / (float)texture->width;
	inverseSize.y = 1.0f / (float)texture->height;

	for( int i = 0; i < count; ++i ) {
		int newIdx = findAvailableImageIndex( );
		if( newIdx < 0 ) {
			llog( LOG_ERROR, "Problem finding available image to split into." );
			img_CleanPackage( packageID );
			return -1;
		}

		images[newIdx].textureObj = texture->textureID;
		vec2_Subtract( &( maxes[i] ), &( mins[i] ), &( images[newIdx].size ) );
		images[newIdx].offset = VEC2_ZERO;
		images[newIdx].packageID = packageID;
		images[newIdx].flags = IMGFLAG_IN_USE;
		vec2_HadamardProd( &( mins[i] ), &inverseSize, &( images[newIdx].uvMin ) );
		vec2_HadamardProd( &( maxes[i] ), &inverseSize, &( images[newIdx].uvMax ) );
		images[newIdx].shaderType = shaderType;
		if( texture->flags & TF_IS_TRANSPARENT ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}

		if( ( imgIDs != NULL ) && ( imgIDs[i] != NULL ) ) {
			hashMap_Set( &imgIDMap, imgIDs[i], newIdx );
		}

		retIDs[i] = newIdx;
	}

	return 0;
}

/*
Takes in a file name and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitImageFile( char* fileName, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_LoadTexture( fileName, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem loading image %s", fileName );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, imgIDs, retIDs ) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Takes in an RGBA bitmap and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitRGBABitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_CreateTextureFromRGBABitmap( data, width, height, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem creating RGBA bitmap texture." );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, NULL, retIDs ) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Takes in a one channel bitmap and some rectangles. It's assume the length of the mins, maxes, and retIDs equal scount.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitAlphaBitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_CreateTextureFromAlphaBitmap( data, width, height, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem creating alpha bitmap texture." );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, NULL, retIDs) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Cleans up all the images in an image package.
*/
void img_CleanPackage( int packageID )
{
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID == packageID ) ) {
			img_Clean( i );
		}
	}
}

/*
Sets an offset to render the image from. The default is the center of the image.
*/
void img_SetOffset( int idx, Vector2 offset )
{
	assert( idx < MAX_IMAGES );
	assert( idx >= 0 );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	images[idx].offset = offset;
}

#include "../Utils/helpers.h"
void img_ForceTransparency( int idx, bool transparent )
{
	assert( idx < MAX_IMAGES );
	assert( idx >= 0 );

	if( !( images[idx].flags & IMGFLAG_IN_USE ) ) {
		return;
	}

	if( transparent ) {
		TURN_ON_BITS( images[idx].flags, IMGFLAG_HAS_TRANSPARENCY );
	} else {
		TURN_OFF_BITS( images[idx].flags, IMGFLAG_HAS_TRANSPARENCY );
	}
}

/*
Gets the size of the image, putting it into the out Vector2. Returns a negative number if there's an issue.
*/
int img_GetSize( int idx, Vector2* out )
{
	assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return -1;
	}

	(*out) = images[idx].size;
	return 0;
}

/*
Gets the texture id for the image, used if you need to render it directly instead of going through this.
 Returns whether out was successfully set or not.
*/
int img_GetTextureID( int idx, GLuint* out )
{
	assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return -1;
	}

	(*out) = images[idx].textureObj;
	return 0;
}

// Retrieves a loaded image by it's id, for images loaded from files this will be the local path, for sprite sheet images 
int img_GetExistingByID( const char* id )
{
	int idx;
	if( !hashMap_Find( &imgIDMap, id, &idx ) ) {
		return -1;
	}
	return idx;
}

/*
initializes the structure so only what's used needs to be set, also fill in
  some stuff that all of the queueRenderImage functions use, returns the pointer
  for further setting of other stuff
 returns NULL if there's a problem
*/
static DrawInstruction* GetNextRenderInstruction( int imgObj, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth )
{
	// the image hasn't been loaded yet, so don't render anything
	if( imgObj < 0 ) {
		// TODO: Use a temporary image.
		return NULL;
	}

	if( !( images[imgObj].flags & IMGFLAG_IN_USE ) ) {
		llog( LOG_VERBOSE, "Attempting to draw invalid image: %i", imgObj );
		return NULL;
	}

	if( lastDrawInstruction >= MAX_RENDER_INSTRUCTIONS ) {
		llog( LOG_VERBOSE, "Render instruction queue full." );
		return NULL;
	}

	++lastDrawInstruction;
	DrawInstruction* ri = &( renderBuffer[lastDrawInstruction] );

	*ri = DEFAULT_DRAW_INSTRUCTION;
	ri->textureObj = images[imgObj].textureObj;
	ri->imageObj = imgObj;
	ri->start.pos = startPos;
	ri->end.pos = endPos;
	ri->start.scaledSize = images[imgObj].size;
	ri->end.scaledSize = images[imgObj].size;
	ri->start.color = CLR_WHITE;
	ri->end.color = CLR_WHITE;
	ri->start.rotation = 0.0f;
	ri->end.rotation = 0.0f;
	ri->start.offset = images[imgObj].offset;
	ri->end.offset = images[imgObj].offset;
	ri->start.floatVal0 = 0.0f;
	ri->end.floatVal0 = 0.0f;
	ri->flags = images[imgObj].flags;
	ri->camFlags = camFlags;
	ri->shaderType = images[imgObj].shaderType;
	ri->depth = depth;
	ri->stencilID = -1;
	ri->isStencil = false;
	memcpy( ri->uvs, DEFAULT_DRAW_INSTRUCTION.uvs, sizeof( ri->uvs ) );

	ri->uvs[0] = images[imgObj].uvMin;

	ri->uvs[1].x = images[imgObj].uvMin.x;
	ri->uvs[1].y = images[imgObj].uvMax.y;

	ri->uvs[2].x = images[imgObj].uvMax.x;
	ri->uvs[2].y = images[imgObj].uvMin.y;

	ri->uvs[3] = images[imgObj].uvMax;

	return ri;
}

int img_CreateDraw( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth )
{
	// the image hasn't been loaded yet, so don't render anything
	if( imgID < 0 ) {
		// TODO: Use a temporary image.
		return -1;
	}

	if( !( images[imgID].flags & IMGFLAG_IN_USE ) ) {
		llog( LOG_VERBOSE, "Attempting to draw invalid image: %i", imgID );
		return -1;
	}

	if( lastDrawInstruction >= MAX_RENDER_INSTRUCTIONS ) {
		llog( LOG_VERBOSE, "Render instruction queue full." );
		return -1;
	}

	++lastDrawInstruction;
	DrawInstruction* ri = &( renderBuffer[lastDrawInstruction] );

	*ri = DEFAULT_DRAW_INSTRUCTION;
	ri->textureObj = images[imgID].textureObj;
	ri->imageObj = imgID;
	ri->start.pos = startPos;
	ri->end.pos = endPos;
	ri->start.scaledSize = images[imgID].size;
	ri->end.scaledSize = images[imgID].size;
	ri->start.color = CLR_WHITE;
	ri->end.color = CLR_WHITE;
	ri->start.rotation = 0.0f;
	ri->end.rotation = 0.0f;
	ri->start.offset = images[imgID].offset;
	ri->end.offset = images[imgID].offset;
	ri->start.floatVal0 = 0.0f;
	ri->end.floatVal0 = 0.0f;
	ri->flags = images[imgID].flags;
	ri->camFlags = camFlags;
	ri->shaderType = images[imgID].shaderType;
	ri->depth = depth;
	ri->stencilID = -1;
	ri->isStencil = false;
	memcpy( ri->uvs, DEFAULT_DRAW_INSTRUCTION.uvs, sizeof( ri->uvs ) );

	ri->uvs[0] = images[imgID].uvMin;

	ri->uvs[1].x = images[imgID].uvMin.x;
	ri->uvs[1].y = images[imgID].uvMax.y;

	ri->uvs[2].x = images[imgID].uvMax.x;
	ri->uvs[2].y = images[imgID].uvMin.y;

	ri->uvs[3] = images[imgID].uvMax;

	return lastDrawInstruction;
}

void img_SetDrawScale( int drawID, float start, float end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set scale on invalid draw command." );
		return;
	}

	renderBuffer[drawID].start.scaledSize.x *= start;
	renderBuffer[drawID].start.scaledSize.y *= start;
	renderBuffer[drawID].end.scaledSize.x *= end;
	renderBuffer[drawID].end.scaledSize.y *= end;
	renderBuffer[drawID].start.offset.x *= start;
	renderBuffer[drawID].start.offset.y *= start;
	renderBuffer[drawID].end.offset.x *= end;
	renderBuffer[drawID].end.offset.y *= end;
}

void img_SetDrawScaleV( int drawID, Vector2 start, Vector2 end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set scale on invalid draw command." );
		return;
	}

	renderBuffer[drawID].start.scaledSize.x *= start.x;
	renderBuffer[drawID].start.scaledSize.y *= start.y;
	renderBuffer[drawID].end.scaledSize.x *= end.x;
	renderBuffer[drawID].end.scaledSize.y *= end.y;
	renderBuffer[drawID].start.offset.x *= start.x;
	renderBuffer[drawID].start.offset.y *= start.y;
	renderBuffer[drawID].end.offset.x *= end.x;
	renderBuffer[drawID].end.offset.y *= end.y;
}

void img_SetDrawColor( int drawID, Color start, Color end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set color on invalid draw command." );
		return;
	}

	renderBuffer[drawID].start.color = start;
	renderBuffer[drawID].end.color = end;

	if( ( ( start.a > 0.0f ) && ( start.a < 1.0f ) ) ||
		( ( end.a > 0.0f ) && ( end.a < 1.0f ) ) ) {
		renderBuffer[drawID].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
}

void img_SetDrawRotation( int drawID, float start, float end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set rotation on invalid draw command." );
		return;
	}

	renderBuffer[drawID].start.rotation = start;
	renderBuffer[drawID].end.rotation = end;
}

void img_SetDrawFloatVal0( int drawID, float start, float end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set float val 0 on invalid draw command." );
		return;
	}

	renderBuffer[drawID].start.floatVal0 = start;
	renderBuffer[drawID].end.floatVal0 = end;
}

void img_SetDrawStencil( int drawID, bool isStencil, int stencilID )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set stencil values on invalid draw command." );
		return;
	}

	if( isStencil && ( ( stencilID < 0 ) || ( stencilID > 7 ) ) ) {
		llog( LOG_DEBUG, "Attempting to set invalid stencil id on stencil draw command." );
		return;
	}

	renderBuffer[drawID].isStencil = isStencil;
	renderBuffer[drawID].stencilID = stencilID;
}

void img_SetDrawSize( int drawID, Vector2 start, Vector2 end )
{
	if( ( drawID < 0 ) || ( drawID > lastDrawInstruction ) ) {
		llog( LOG_DEBUG, "Attempting to set size on invalid draw command." );
		return;
	}

	// reset the size
	Vector2 startScale;
	startScale.x = start.x / renderBuffer[drawID].start.scaledSize.x;
	startScale.y = start.y / renderBuffer[drawID].start.scaledSize.y;

	Vector2 endScale;
	endScale.x = start.x / renderBuffer[drawID].end.scaledSize.x;
	endScale.y = start.y / renderBuffer[drawID].end.scaledSize.y;

	img_SetDrawScaleV( drawID, startScale, endScale );
}

/*
Adds to the list of images to draw.
*/
#define DRAW_INSTRUCTION_START \
	DrawInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth ); \
	if( ri == NULL ) { return -1; }

#define DRAW_INSTRUCTION_END \
	return 0;

#define SET_DRAW_INSTRUCTION_SCALE( startX, startY, endX, endY ) \
	ri->start.scaledSize.x *= startX; \
	ri->start.scaledSize.y *= startY; \
	ri->end.scaledSize.x *= endX; \
	ri->end.scaledSize.y *= endY; \
	ri->start.offset.x *= startX; \
	ri->start.offset.y *= startY; \
	ri->end.offset.x *= endX; \
	ri->end.offset.y *= endY;

#define SET_DRAW_INSTRUCTION_COLOR( startColor, endColor ) \
	ri->start.color = startColor; \
	ri->end.color = endColor; \
	if( ( ( startColor.a > 0 ) && ( startColor.a < 1.0f ) ) || \
		( ( endColor.a > 0 ) && ( endColor.a < 1.0f ) )) { \
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY; }

/*
#define SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad ) \
	ri->start.rotation = startRotRad; \
	ri->end.rotation = endRotRad;

#define SET_DRAW_INSTRUCTION_FLOAT_VAL( startVal, endVal ) \
	ri->start.floatVal0 = startVal; \
	ri->end.floatVal0 = endVal;
//*/

int img_Draw_sv_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw3x3( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth )
{
	return img_Draw3x3_c( imgUL, imgUC, imgUR, imgML, imgMC, imgMR, imgDL, imgDC, imgDR,
		camFlags, startPos, endPos, startSize, endSize, CLR_WHITE, CLR_WHITE, depth );
}

int img_Draw3x3v( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth )
{
	return img_Draw3x3( imgs[0], imgs[1], imgs[2], imgs[3], imgs[4], imgs[5], imgs[6], imgs[7], imgs[8],
		camFlags, startPos, endPos, startSize, endSize, depth );
}

int img_Draw3x3_c( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize,
	Color startColor, Color endColor, int8_t depth )
{
	// assuming that the dimensions of each column and row are the same
	// also assumes the width and height to draw greater than the sum of the widths and heights, if it isn't we just
	//  clamp it so it's a minimum of that much

	Vector2 sliceSize;
	img_GetSize( imgUL, &sliceSize );

	float imgLeftColumnWidth = sliceSize.x;
	float imgTopRowHeight = sliceSize.y;

	img_GetSize( imgMC, &sliceSize );

	float imgMidColumnWidth = sliceSize.x;
	float imgMidRowHeight = sliceSize.y;

	img_GetSize( imgDR, &sliceSize );

	float imgRightColumnWidth = sliceSize.x;
	float imgBottomRowHeight = sliceSize.y;

	// calculate the scales
	float midWidthStart = MAX( 0.0f, ( startSize.x - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthEnd = MAX( 0.0f, ( endSize.x - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthScaleStart = midWidthStart / imgMidColumnWidth;
	float midWidthScaleEnd = midWidthEnd / imgMidColumnWidth;

	float midHeightStart = MAX( 0.0f, ( startSize.y - imgTopRowHeight - imgBottomRowHeight ) );
	float midHeightEnd = MAX( 0.0f, ( endSize.y - imgTopRowHeight - imgBottomRowHeight ) );
	float midHeightScaleStart = midHeightStart / imgMidRowHeight;
	float midHeightScaleEnd = midHeightEnd / imgMidRowHeight;

	// calculate the positions
	float leftPosStart = startPos.x - ( midWidthStart / 2.0f ) - ( imgLeftColumnWidth / 2.0f );
	float leftPosEnd = endPos.x - ( midWidthEnd / 2.0f ) - ( imgLeftColumnWidth / 2.0f );
	float midHorizPosStart = startPos.x;
	float midHorizPosEnd = endPos.x;
	float rightPosStart = startPos.x + ( midWidthStart / 2.0f ) + ( imgRightColumnWidth / 2.0f );
	float rightPosEnd = endPos.x + ( midWidthEnd / 2.0f ) + ( imgRightColumnWidth / 2.0f );

	float topPosStart = startPos.y - ( midHeightStart / 2.0f ) - ( imgTopRowHeight / 2.0f );
	float topPosEnd = endPos.y - ( midHeightEnd / 2.0f ) - ( imgTopRowHeight / 2.0f );
	float midVertPosStart = startPos.y;
	float midVertPosEnd = endPos.y;
	float bottomPosStart = startPos.y + ( midHeightStart / 2.0f ) + ( imgBottomRowHeight / 2.0f );
	float bottomPosEnd = endPos.y + ( midHeightEnd / 2.0f ) + ( imgBottomRowHeight / 2.0f );

	Vector2 posStart;
	Vector2 posEnd;
	Vector2 scaleStart;
	Vector2 scaleEnd;

#define DRAW_SECTION( img, startX, startY, endX, endY, startW, startH, endW, endH ) \
	posStart.x = ( startX ); posStart.y = ( startY ); \
	posEnd.x = ( endX ); posEnd.y = ( endY ); \
	scaleStart.w = ( startW ); scaleStart.h = ( startH ); \
	scaleEnd.w = ( endW ); scaleEnd.h = ( endH ); \
	if( img_Draw_sv_c( img, camFlags, posStart, posEnd, scaleStart, scaleEnd, startColor, endColor, depth ) < 0 ) return -1;

	// upper-left
	DRAW_SECTION( imgUL, leftPosStart, topPosStart, leftPosEnd, topPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// upper-center
	DRAW_SECTION( imgUC, midHorizPosStart, topPosStart, midHorizPosEnd, topPosEnd, midWidthScaleStart, 1.0f, midWidthScaleStart, 1.0f );

	// upper-right
	DRAW_SECTION( imgUR, rightPosStart, topPosStart, rightPosEnd, topPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// mid-left
	DRAW_SECTION( imgML, leftPosStart, midVertPosStart, leftPosEnd, midVertPosEnd, 1.0f, midHeightScaleStart, 1.0f, midHeightScaleEnd );

	// mid-center
	DRAW_SECTION( imgMC, midHorizPosStart, midVertPosStart, midHorizPosEnd, midVertPosEnd,
		midWidthScaleStart, midHeightScaleStart, midWidthScaleEnd, midHeightScaleEnd );

	// mid-right
	DRAW_SECTION( imgMR, rightPosStart, midVertPosStart, rightPosEnd, midVertPosEnd, 1.0f, midHeightScaleStart, 1.0f, midHeightScaleEnd );

	// low-left
	DRAW_SECTION( imgDL, leftPosStart, bottomPosStart, leftPosEnd, bottomPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// low-center
	DRAW_SECTION( imgDC, midHorizPosStart, bottomPosStart, midHorizPosEnd, bottomPosEnd, midWidthScaleStart, 1.0f, midWidthScaleStart, 1.0f );

	// low-right
	DRAW_SECTION( imgDR, rightPosStart, bottomPosStart, rightPosEnd, bottomPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

#undef DRAW_SECTION
	return 0;
}

int img_Draw3x3v_c( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos,
	Vector2 startSize, Vector2 endSize, Color startColor, Color endColor, int8_t depth )
{
	return img_Draw3x3_c( imgs[0], imgs[1], imgs[2], imgs[3], imgs[4], imgs[5], imgs[6], imgs[7], imgs[8],
		camFlags, startPos, endPos, startSize, endSize, startColor, endColor, depth );
}

#undef DRAW_INSTRUCTION_QUEUE_START
#undef DRAW_INSTRUCTION_QUEUE_END
#undef SET_DRAW_INSTRUCTION_SCALE
#undef SET_DRAW_INSTRUCTION_COLOR
#undef SET_DRAW_INSTRUCTION_ROT

/*
Clears the image draw list.
*/
void img_ClearDrawInstructions( void )
{
	lastDrawInstruction = -1;
}

static void createRenderTransform( Vector2* pos, Vector2* scale, float rot,  Vector2* offset, Matrix4* out )
{
	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	// this is the fully multiplied out multiplication of the transform, scale, and z-rotation
	// pos * rot * offset * scale
	//  problem here if the scale is different in each dimension (e.g scale = { 16, 24 }), the rotation isn't taken
	//  into account so you end up with an oddly stretched image

	
	float cosRot = cosf( rot );
	float sinRot = sinf( rot );

	out->m[0] = cosRot * scale->v[0];
	out->m[1] = sinRot * scale->v[0];
	out->m[4] = -sinRot * scale->v[1];
	out->m[5] = cosRot * scale->v[1];
	//out->m[10] = scale->v[2];
	out->m[12] = pos->v[0] - ( offset->v[1] * sinRot ) + ( offset->v[0] * cosRot );
	out->m[13] = pos->v[1] + ( offset->v[0] * sinRot ) + ( offset->v[1] * cosRot );
	//out->m[14] = -pos->v[2]; //*/

	// pos * rot * scale * offset
	/*out->m[0] = cosRot * scale->v[0];
	out->m[1] = sinRot * scale->v[0];
	out->m[4] = -sinRot * scale->v[1];
	out->m[5] = cosRot * scale->v[1];

	out->m[12] = pos->v[0] + ( offset->v[0] * scale->v[0] * cosRot ) + ( offset->v[1] * scale->v[1] * -sinRot );
	out->m[13] = pos->v[1] + ( offset->v[0] * scale->v[0] * sinRot ) + ( offset->v[1] * scale->v[1] * cosRot );//*/
}

/*
Draw all the images.
*/
void img_Render( float normTimeElapsed )
{
	Vector2 unitSqVertPos[] = { { -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f } };
	GLuint indices[] = {
		0, 1, 2,
		1, 2, 3,
	};

	for( int idx = 0; idx <= lastDrawInstruction; ++idx ) {
		TriVert verts[4];

		// generate the sprites matrix
		Matrix4 modelTf;
		Vector2 pos, sclSz, offset;
		float rot;
		Color col;
		float floatVal0;

		vec2_Lerp( &( renderBuffer[idx].start.pos ), &( renderBuffer[idx].end.pos ), normTimeElapsed, &pos );
		clr_Lerp( &( renderBuffer[idx].start.color ), &( renderBuffer[idx].end.color ), normTimeElapsed, &col );
		vec2_Lerp( &( renderBuffer[idx].start.scaledSize ), &( renderBuffer[idx].end.scaledSize ), normTimeElapsed, &sclSz );
		vec2_Lerp( &( renderBuffer[idx].start.offset ), &( renderBuffer[idx].end.offset ), normTimeElapsed, &offset );
		floatVal0 = lerp( renderBuffer[idx].start.floatVal0, renderBuffer[idx].end.floatVal0, normTimeElapsed );

		rot = radianRotLerp( renderBuffer[idx].start.rotation, renderBuffer[idx].end.rotation, normTimeElapsed );
		createRenderTransform( &pos, &sclSz, rot, &offset, &modelTf );

		for( int i = 0; i < 4; ++i ) {
			mat4_TransformVec2Pos( &modelTf, &( unitSqVertPos[i] ), &( verts[i].pos ) );
			verts[i].uv = renderBuffer[idx].uvs[i];
			verts[i].col = col;
		}

		TriType type = ( renderBuffer[idx].flags & IMGFLAG_HAS_TRANSPARENCY ) ? TT_TRANSPARENT : TT_SOLID;
		if( renderBuffer[idx].isStencil ) {
			type = TT_STENCIL;
		}

		triRenderer_Add( verts[indices[0]], verts[indices[1]], verts[indices[2]],
			renderBuffer[idx].shaderType, renderBuffer[idx].textureObj, floatVal0,
			renderBuffer[idx].stencilID, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			type );
		triRenderer_Add( verts[indices[3]], verts[indices[4]], verts[indices[5]],
			renderBuffer[idx].shaderType, renderBuffer[idx].textureObj, floatVal0,
			renderBuffer[idx].stencilID, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			type );
	}
}