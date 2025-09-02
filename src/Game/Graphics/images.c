#include "images.h"

#include <math.h>
#include <stdlib.h>

#include "Math/matrix4.h"
#include "gfxUtil.h"
#include "Graphics/Platform/graphicsPlatform.h"
#include "System/platformLog.h"
#include "Math/mathUtil.h"

#include "System/jobQueue.h"
#include "System/jobRingQueue.h"
#include "System/memory.h"

#include "Utils/stretchyBuffer.h"
#include "Utils/helpers.h"

/* Image loading types and variables */
#define MAX_IMAGES 512

enum {
	IMGFLAG_IN_USE = 0x1,
	IMGFLAG_HAS_TRANSPARENCY = 0x2,
};

typedef struct {
	PlatformTexture textureObj;
	Vector2 uvMin;
	Vector2 uvMax;
	Vector2 size;
	Vector2 offset; // determines where the image is drawn from and rotated around, defaults to the center of the image
	int flags;
	int packageID;
	int nextInPackage;
	ShaderType shaderType;
	int extraImageObj;
	char* id;
} Image;

static Image images[MAX_IMAGES];

// Rendering types and variables
static int maxTextureSize;

// Initializes images.
bool img_Init( void )
{
    maxTextureSize = gfxPlatform_GetMaxTextureSize( );
	memset( images, 0, sizeof(images) );

	// create a default white 4x4 square to use
	uint8_t whiteImgData[] = {
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
	};
	Texture whiteImgTexture;
	gfxUtil_CreateTextureFromRGBABitmap( whiteImgData, 4, 4, &whiteImgTexture );
	img_CreateFromTexture( &whiteImgTexture, ST_DEFAULT, "default_white_square" );

	return true;
}

// Finds the first unused image index.
//  Returns a postive value on success, a negative on failure.
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

bool findImageByID( const char* id, int* outIdx )
{
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( !( images[i].flags & IMGFLAG_IN_USE ) ) {
			continue;
		}

		if( images[i].id != NULL && SDL_strcmp( id, images[i].id ) == 0 ) {
			( *outIdx ) = i;
			return true;
		}
	}

	return false;
}

// Loads the image stored at file name.
//  Returns the index of the image on success.
//  Returns -1 on failure, and prints a message to the log.
int img_Load( const char* fileName, ShaderType shaderType )
{
	int newIdx = -1;

	// if we've already loaded the image don't load it again
	if( findImageByID( fileName, &newIdx ) ) {
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

	images[newIdx].textureObj = texture.texture;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	images[newIdx].extraImageObj = -1;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
	images[newIdx].id = createStringCopy( fileName );

	return newIdx;
}

// Returns whether imgIdx points to a valid image.
bool img_IsValidImage( int imgIdx )
{
	if( imgIdx >= MAX_IMAGES ) return false;
	if( imgIdx < 0 ) return false;
	return images[imgIdx].flags & IMGFLAG_IN_USE;
}

int img_CreateFromLoadedImage( LoadedImage* loadedImg, ShaderType shaderType, const char* id )
{
	int newIdx = -1;

	// if we've already loaded the image with this id don't load it again
	if( findImageByID( id, &newIdx ) ) {
		return newIdx;
	}

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image! Image storage full." );
		return -1;
	}

	Texture texture;
	if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, loadedImg, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to create image!" );
		return -1;
	}

	images[newIdx].textureObj = texture.texture;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	images[newIdx].extraImageObj = -1;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
	images[newIdx].id = createStringCopy( id );

	return newIdx;
}

int img_CreateFromTexture( Texture* texture, ShaderType shaderType, const char* id )
{
	int newIdx = -1;;

	// if we've already loaded the image with this id don't load it again
	if( findImageByID( id, &newIdx ) ) {
		return newIdx;
	}

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image! Image storage full." );
		return -1;
	}

	images[newIdx].textureObj = texture->texture;
	images[newIdx].size.v[0] = (float)texture->width;
	images[newIdx].size.v[1] = (float)texture->height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	images[newIdx].extraImageObj = -1;
	if( texture->flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
	images[newIdx].id = createStringCopy( id );

	return newIdx;
}

typedef struct {
	char* fileName;
	ShaderType shaderType;
	int* outIdx;
	LoadedImage loadedImage;
	void ( *onLoadDone )( int );
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
	if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, &( loadData->loadedImage ), &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to bind image %s!", loadData->fileName );
		goto clean_up;
	}

	//llog( LOG_INFO, "Done loading %s", loadData->fileName );

	images[newIdx].textureObj = texture.texture;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = loadData->shaderType;
	images[newIdx].extraImageObj = -1;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}
	images[newIdx].id = createStringCopy( loadData->fileName );

	(*(loadData->outIdx)) = newIdx;

	//llog( LOG_INFO, "Setting outIdx to %i", newIdx );

clean_up:
	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( *(loadData->outIdx) );
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
void img_ThreadedLoad( const char* fileName, ShaderType shaderType, int* outIdx, void (*onLoadDone)( int ) )
{
	SDL_assert( fileName != NULL );

	// if we've already loaded this image don't load it again
	if( findImageByID( fileName, outIdx ) ) {
		return;
	}

	// set it to something that won't draw assert
	(*outIdx) = -1;

	// this isn't something that should be happening all the time, so allocating and freeing
	//  should be fine, if we want to do continous streaming it would probably be better to
	//  make a specialty queue for it
	ThreadedLoadImageData* data = mem_Allocate( sizeof( ThreadedLoadImageData ) );
	if( data == NULL ) {
		llog( LOG_WARN, "Unable to create data for threaded image load for file %s", fileName );
		if( onLoadDone != NULL ) onLoadDone( -1 );
		return;
	}

	size_t fileNameLen = strlen( fileName );
	data->fileName = mem_Allocate( fileNameLen + 1 );
	if( data->fileName == NULL ) {
		llog( LOG_WARN, "Unable to create file name storage for threaded image lead for fle %s", fileName );
		mem_Release( data );
		if( onLoadDone != NULL ) onLoadDone( -1 );
		return;
	}
	SDL_strlcpy( data->fileName, fileName, fileNameLen + 1 );

	data->shaderType = shaderType;
	data->outIdx = outIdx;
	data->loadedImage.data = NULL;
	data->onLoadDone = onLoadDone;

	if( !jq_AddJob( loadImageJob, data ) ) {
		mem_Release( data );
		if( onLoadDone != NULL ) onLoadDone( -1 );
	}
}

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface, ShaderType shaderType, const char* id )
{
	int newIdx;

	// if we've already loaded the image with this id don't load it again
	if( findImageByID( id, &newIdx ) ) {
		return newIdx;
	}

	SDL_assert( surface != NULL );

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image from surface! Image storage full." );
		return -1;
	}

	Texture texture;
	if( gfxPlatform_CreateTextureFromSurface( surface, &texture ) ) {
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
		images[newIdx].extraImageObj = -1;
		if( texture.flags & TF_IS_TRANSPARENT ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}
		images[newIdx].id = createStringCopy( id );
	}

	return newIdx;
}

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void img_Clean( int idx )
{
	SDL_assert( idx < MAX_IMAGES );
	SDL_assert( idx >= 0 );
	SDL_assert( images[idx].flags & IMGFLAG_IN_USE );

	if( ( idx < 0 ) || ( ( images[idx].size.v[0] == 0.0f ) && ( images[idx].size.v[1] == 0.0f ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	// see if this is the last image using that texture
	//  TODO: See if this needs to be sped up
	int deleteTexture = 1;
	for( int i = 0; ( i < MAX_IMAGES ) && deleteTexture; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( gfxPlatform_ComparePlatformTextures( images[i].textureObj, images[idx].textureObj ) == 0 ) ) {
			deleteTexture = 0;
		}
	}

	if( deleteTexture ) {
		gfxPlatform_DeletePlatformTexture( images[idx].textureObj );
		
	}
	images[idx].size = VEC2_ZERO;
	images[idx].flags = 0;
	images[idx].packageID = -1;
	images[idx].nextInPackage = -1;
	images[idx].uvMin = VEC2_ZERO;
	images[idx].uvMax = VEC2_ZERO;
	images[idx].shaderType = ST_DEFAULT;
	mem_Release( images[idx].id );
}

/*
Finds the next unused package ID.
*/
int findUnusedPackage( void )
{
	int packageID = 0;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID >= packageID ) ) {
			packageID = images[i].packageID + 1;
		}
	}
	return packageID;
}

/*
Splits the texture. Returns a negative number if there's a problem.
*/
static int split( Texture* texture, int packageID, ShaderType shaderType, int count, Vector2* mins, Vector2* maxes, char** imgIDs, int* retIDs )
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

		images[newIdx].textureObj = texture->texture;
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
		if( imgIDs != NULL ) {
			images[newIdx].id = createStringCopy( imgIDs[i] );
		} else {
			images[newIdx].id = NULL; // TODO: Create a random UUID to use.
		}

		if( retIDs != NULL ) {
			retIDs[i] = newIdx;
		}
	}

	return 0;
}

int img_SplitTexture( Texture* texture, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, int packageID, int* retIDs )
{
	int currentPackageID = packageID;
	if( currentPackageID < 0 ) {
		currentPackageID = findUnusedPackage( );
	}

	if( split( texture, currentPackageID, shaderType, count, mins, maxes, imgIDs, retIDs ) < 0 ) {
		return -1;
	}

	return currentPackageID;
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

// Gets all the images associated with the packageID and returns a stretchy buffer containing them.
int* img_GetPackageImages( int packageID )
{
	int* sbImgs = NULL;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID == packageID ) ) {
			sb_Push( sbImgs, i );
		}
	}
	return sbImgs;
}

// Gets all the number of images associated with the packageID.
size_t img_GetPackageImageCount( int packageID )
{
	size_t count = 0;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID == packageID ) ) {
			++count;
		}
	}
	return count;
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
	SDL_assert( idx < MAX_IMAGES );
	SDL_assert( idx >= 0 );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	images[idx].offset = offset;
}

// Sets an offset based on a vector with the ranges [0,1], default is <0.5, 0.5>, 0 is left and top, 1 is right and bottom
//  padding is for if you want some changes based on pixels and not a ratio
void img_SetRatioOffset( int idx, Vector2 offsetRatio, Vector2 padding )
{
	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	offsetRatio.x = ( 1.0f - offsetRatio.x ) - 0.5f;
	offsetRatio.y = ( 1.0f - offsetRatio.y ) - 0.5f;
	Vector2 size = images[idx].size;
	Vector2 offset;
	vec2_HadamardProd( &size, &offsetRatio, &offset );
	vec2_Add( &offset, &padding, &offset );
	images[idx].offset = offset;
}

void img_GetOffset( int idx, Vector2* out )
{
	SDL_assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	(*out) = images[idx].offset;
}

void img_ForceTransparency( int idx, bool transparent )
{
	SDL_assert( idx < MAX_IMAGES );
	SDL_assert( idx >= 0 );

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
bool img_GetSize( int idx, Vector2* out )
{
	SDL_assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return false;
	}

	(*out) = images[idx].size;
	return true;
}

// returns the ShaderType of the image
ShaderType img_GetShaderType( int idx )
{
	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return NUM_SHADERS;
	}
	return images[idx].shaderType;
}

// used to override the ShaderType value used when the image was loaded, helpful for sprite sheets
void img_SetShaderType( int idx, ShaderType shaderType )
{
	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}
	images[idx].shaderType = shaderType;
}

// Gets the the min and max uv coordinates used by the image.
int img_GetUVCoordinates( int idx, Vector2* outMin, Vector2* outMax )
{
	SDL_assert( outMin != NULL );
	SDL_assert( outMax != NULL );

	if( !img_IsValidImage( idx ) ) {
		return -1;
	}

	( *outMin ) = images[idx].uvMin;
	( *outMax ) = images[idx].uvMax;
	return 0;
}

// Gets a scale to use for the image to get a desired size.
int img_GetDesiredScale( int idx, Vector2 desiredSize, Vector2* outScale )
{
	SDL_assert( outScale != NULL );
	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return -1;
	}

	(*outScale) = images[idx].size;

	outScale->x = desiredSize.x / outScale->x;
	outScale->y = desiredSize.y / outScale->y;

	return 0;
}

/*
Gets the texture id for the image, used if you need to render it directly instead of going through this.
 Returns whether out was successfully set or not.
*/
int img_GetTextureID( int idx, PlatformTexture* out )
{
	SDL_assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return -1;
	}

	(*out) = images[idx].textureObj;
	return 0;
}

// Retrieves a loaded image by it's id, for images loaded from files this will be the local path, for sprite sheet images 
int img_GetExistingByID( const char* id )
{
	int imgID;
	if( findImageByID( id, &imgID ) ) {
		return imgID;
	}
	return -1;
}

// Sets the image at colorIdx to use use alphaIdx as a signed distance field alpha map
int img_SetSDFAlphaMap( int colorIdx, int alphaIdx )
{
	if( ( colorIdx < 0 ) || ( !( images[colorIdx].flags & IMGFLAG_IN_USE ) ) || ( colorIdx >= MAX_IMAGES ) ) {
		return -1;
	}

	if( ( alphaIdx < 0 ) || ( !( images[alphaIdx].flags & IMGFLAG_IN_USE ) ) || ( alphaIdx >= MAX_IMAGES ) ) {
		return -1;
	}

	images[colorIdx].shaderType = ST_ALPHA_MAPPED_SDF;
	images[colorIdx].extraImageObj = alphaIdx;
	images[colorIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;

	return 0;
}

const char* img_GetImgStringID( int imgID )
{
	if( !img_IsValidImage( imgID ) ) return NULL;
	return images[imgID].id;
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

// Get the id of the first valid image. Returns -1 if there are none.
int img_FirstValidID( void )
{
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( img_IsValidImage( i ) ) {
			return i;
		}
	}

	return -1;
}

// Gets the next valid image after the one passed in. Returns -1 if there are none.
int img_NextValidID( int id )
{
	for( int i = id + 1; i < MAX_IMAGES; ++i ) {
		if( img_IsValidImage( i ) ) {
			return i;
		}
	}

	return -1;
}

ImageRenderInstruction img_CreateDefaultRenderInstruction( void )
{
	ImageRenderInstruction ri;

	ri.imgID = -1;
	ri.camFlags = 0;
	ri.depth = 0;
	ri.mat = IDENTITY_MATRIX_3;
	ri.color = CLR_WHITE;
	ri.val0 = 0.0f;
	ri.isStencil = false;
	ri.stencilID = -1;
	ri.subSectionTopLeftNorm.x = ri.subSectionTopLeftNorm.y = 0.0f;
	ri.subSectionBottomRightNorm.x = ri.subSectionBottomRightNorm.y = 1.0f;

	return ri;
}

ImageRenderInstruction img_CreateRenderInstruction_Pos( int imgID, uint32_t camFlags, const Vector2* pos, int8_t depth )
{
	SDL_assert( pos != NULL );

	ImageRenderInstruction ri;

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;

	ri.color = CLR_WHITE;
	ri.val0 = 0.0f;
	ri.isStencil = false;
	ri.stencilID = -1;

	ri.subSectionTopLeftNorm.x = ri.subSectionTopLeftNorm.y = 0.0f;
	ri.subSectionBottomRightNorm.x = ri.subSectionBottomRightNorm.y = 1.0f;

	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	return ri;
}

ImageRenderInstruction img_CreateRenderInstruction_PosRot( int imgID, uint32_t camFlags, const Vector2* pos, float rotRad, int8_t depth )
{
	SDL_assert( pos != NULL );

	ImageRenderInstruction ri;

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;

	ri.color = CLR_WHITE;
	ri.val0 = 0.0f;
	ri.isStencil = false;
	ri.stencilID = -1;

	ri.subSectionTopLeftNorm.x = ri.subSectionTopLeftNorm.y = 0.0f;
	ri.subSectionBottomRightNorm.x = ri.subSectionBottomRightNorm.y = 1.0f;

	mat3_CreateRenderTransform( pos, rotRad, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	return ri;
}

ImageRenderInstruction img_CreateRenderInstruction_PosVScale( int imgID, uint32_t camFlags, const Vector2* pos, const Vector2* scale, int8_t depth )
{
	SDL_assert( pos != NULL );
	SDL_assert( scale != NULL );

	ImageRenderInstruction ri;

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;

	ri.color = CLR_WHITE;
	ri.val0 = 0.0f;
	ri.isStencil = false;
	ri.stencilID = -1;

	ri.subSectionTopLeftNorm.x = ri.subSectionTopLeftNorm.y = 0.0f;
	ri.subSectionBottomRightNorm.x = ri.subSectionBottomRightNorm.y = 1.0f;

	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, scale, &( ri.mat ) );

	return ri;
}

void img_ImmediateRender( ImageRenderInstruction* instruction )
{
	Vector2 unitSqVertPos[] = { { -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f } };
	unsigned int indices[] = {
		0, 1, 2,
		1, 2, 3,
	};

	int imgID = instruction->imgID;
	bool isValidImg = img_IsValidImage( imgID );
	ASSERT_AND_IF_NOT( isValidImg ) {
		return;
	}
	
	Vector2 imgSize;
	img_GetSize( imgID, &imgSize );
	imgSize.w = imgSize.w * ( instruction->subSectionBottomRightNorm.x - instruction->subSectionTopLeftNorm.x );
	imgSize.h = imgSize.h * ( instruction->subSectionBottomRightNorm.y - instruction->subSectionTopLeftNorm.y );

	TriVert verts[4];

	// generate the uv coords based on the image
	Vector2 uvs[4];
	uvs[0].x = lerp( images[imgID].uvMin.x, images[imgID].uvMax.x, instruction->subSectionTopLeftNorm.x );
	uvs[0].y = lerp( images[imgID].uvMin.y, images[imgID].uvMax.y, instruction->subSectionTopLeftNorm.y );

	uvs[1].x = lerp( images[imgID].uvMin.x, images[imgID].uvMax.x, instruction->subSectionTopLeftNorm.x );
	uvs[1].y = lerp( images[imgID].uvMin.y, images[imgID].uvMax.y, instruction->subSectionBottomRightNorm.y );

	uvs[2].x = lerp( images[imgID].uvMin.x, images[imgID].uvMax.x, instruction->subSectionBottomRightNorm.x );
	uvs[2].y = lerp( images[imgID].uvMin.y, images[imgID].uvMax.y, instruction->subSectionTopLeftNorm.y );

	uvs[3].x = lerp( images[imgID].uvMin.x, images[imgID].uvMax.x, instruction->subSectionBottomRightNorm.x );
	uvs[3].y = lerp( images[imgID].uvMin.y, images[imgID].uvMax.y, instruction->subSectionBottomRightNorm.y );

	// get the verts to use for rendering
	for( int i = 0; i < 4; ++i ) {
		vec2_HadamardProd( &unitSqVertPos[i], &imgSize, &unitSqVertPos[i] );
		mat3_TransformVec2Pos( &(instruction->mat), &( unitSqVertPos[i] ), &( verts[i].pos ) );
		verts[i].uv = uvs[i];
		verts[i].col = instruction->color;
	}

	// check stencil
	TriType type = ( ( images[imgID].flags & IMGFLAG_HAS_TRANSPARENCY ) || ( instruction->color.a != 1.0f ) ) ? TT_TRANSPARENT : TT_SOLID;
	if( instruction->isStencil ) {
		type = TT_STENCIL;
	}

	PlatformTexture extraTexture;
	int extraImageObj = images[imgID].extraImageObj;
	if( extraImageObj >= 0 ) {
		extraTexture = images[extraImageObj].textureObj;
	} else {
		extraTexture = gfxPlatform_GetDefaultPlatformTexture( );
	}

	triRenderer_Add( verts[indices[0]], verts[indices[1]], verts[indices[2]],
		images[imgID].shaderType, images[imgID].textureObj, extraTexture, instruction->val0,
		instruction->stencilID, instruction->camFlags, instruction->depth,
		type );
	triRenderer_Add( verts[indices[3]], verts[indices[4]], verts[indices[5]],
		images[imgID].shaderType, images[imgID].textureObj, extraTexture, instruction->val0,
		instruction->stencilID, instruction->camFlags, instruction->depth,
		type );
}

bool img_SetRenderInstructionBorders( ImageRenderInstruction* instruction, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom )
{
	ASSERT_AND_IF_NOT( instruction != NULL ) return false;
	bool isValid = img_IsValidImage( instruction->imgID );
	ASSERT_AND_IF_NOT( isValid ) return false;
	ASSERT_AND_IF_NOT( left <= right ) return false;
	ASSERT_AND_IF_NOT( top <= bottom ) return false;

	// first make sure the borders fit inside the image, if we would create an image that wouldn't be renderable then don't set it
	Vector2 imgSize;
	img_GetSize( instruction->imgID, &imgSize );

	ASSERT_AND_IF_NOT( left <= imgSize.w ) return false;
	ASSERT_AND_IF_NOT( right <= imgSize.w ) return false;
	ASSERT_AND_IF_NOT( top <= imgSize.h ) return false;
	ASSERT_AND_IF_NOT( bottom <= imgSize.h ) return false;

//#error TODO: Get this working with the actual rendering
	instruction->subSectionTopLeftNorm.x = (float)left / imgSize.w;
	instruction->subSectionTopLeftNorm.y = (float)top / imgSize.h;

	instruction->subSectionBottomRightNorm.x = (float)right / imgSize.w;
	instruction->subSectionBottomRightNorm.y = (float)bottom / imgSize.h;

	return true;
}

void img_Render_Pos( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos )
{
	SDL_assert( pos != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;

	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	img_ImmediateRender( &ri );
}

void img_Render_PosClr( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, const Color* clr )
{
	SDL_assert( pos != NULL );
	SDL_assert( clr != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;
	ri.color = *clr;

	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	img_ImmediateRender( &ri );
}

void img_Render_PosRot( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad )
{
	SDL_assert( pos != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;

	mat3_CreateRenderTransform( pos, rotRad, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	img_ImmediateRender( &ri );
}

void img_Render_PosRotClr( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad, const Color* clr )
{
	SDL_assert( pos != NULL );
	SDL_assert( clr != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;
	ri.color = ( *clr );

	mat3_CreateRenderTransform( pos, rotRad, &VEC2_ZERO, &VEC2_ONE, &( ri.mat ) );

	img_ImmediateRender( &ri );
}

void img_Render_PosScaleVClr( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, const Vector2* scale, const Color* clr )
{
	SDL_assert( pos != NULL );
	SDL_assert( clr != NULL );
	SDL_assert( scale != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;
	ri.color = ( *clr );

	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, scale, &( ri.mat ) );

	img_ImmediateRender( &ri );
}

void img_Render_PosRotScaleClr( int imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad, float scale, const Color* clr )
{
	SDL_assert( pos != NULL );
	SDL_assert( clr != NULL );

	ImageRenderInstruction ri = img_CreateDefaultRenderInstruction( );

	ri.imgID = imgID;
	ri.camFlags = camFlags;
	ri.depth = depth;
	ri.color = ( *clr );

	Vector2 scaleVec = vec2( scale, scale );
	mat3_CreateRenderTransform( pos, 0.0f, &VEC2_ZERO, &scaleVec, &( ri.mat ) );

	img_ImmediateRender( &ri );
}