#include "gfxUtil.h"

#include <SDL_endian.h>


// gets sbt_image.h to use our custom memory allocation
#include <stdlib.h>
#include "System/memory.h"
#define free(ptr) mem_Release(ptr)
#define realloc(ptr,size) mem_Resize(ptr,size)
#define malloc(size) mem_Allocate(size)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef malloc
#undef free
#undef realloc

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>


#define STB_IMAGE_WRITE_IMPLEMENTATION
//#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(sz)        mem_Allocate(sz)
#define STBIW_REALLOC(p,newsz)  mem_Resize(p,newsz)
#define STBIW_FREE(p)           mem_Release(p)

#pragma warning( push )
#pragma warning( disable : 4204 )
#include <stb_image_write.h>
#pragma warning( pop )

#include "Graphics/graphics.h"
#include "Graphics/Platform/graphicsPlatform.h"

#include "System/platformLog.h"

// Clean up anything that was created in a loaded image.
void gfxUtil_ReleaseLoadedImage( LoadedImage* image )
{
	assert( image != NULL );

	stbi_image_free( image->data );
}

// Loads the data from fileName into outLoadedImage, used as an intermediary between loading and creating a texture.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadImage( const char* fileName, LoadedImage* outLoadedImage )
{
	if( outLoadedImage == NULL ) {
		llog( LOG_INFO, "Attempting to load an image without a place to store it! %s", fileName );
		return -1;
	}

	outLoadedImage->reqComp = 4;
	outLoadedImage->data = NULL;

#if defined( __ANDROID__ )
	// android has the assets stored in the apk, so we'll have to access that, SDL will handle whether it's a file stored in
	//  data or the package
	uint8_t* buffer;

	SDL_RWops* file = SDL_RWFromFile( fileName, "r" );
	if( file == NULL ) {
		llog( LOG_INFO, "Unable to load image %s!", fileName );
		return -1;
	}
	size_t fileSize = (size_t)SDL_RWsize( file );
	buffer = mem_Allocate( fileSize );

	size_t readTotal = 0;
	size_t amtRead = 1;
	uint8_t* bufferPos = buffer;
	while( ( readTotal < fileSize ) && ( amtRead > 0 ) ) {
		amtRead = SDL_RWread( file, bufferPos, sizeof( uint8_t ), ( fileSize - readTotal ) );
		readTotal += amtRead;
		bufferPos += amtRead;
	}
	SDL_RWclose( file );

	outLoadedImage->data = stbi_load_from_memory( buffer, (int)fileSize,
		&( outLoadedImage->width ), &( outLoadedImage->height ), &( outLoadedImage->comp ), outLoadedImage->reqComp );

	mem_Release( buffer );
#else
	outLoadedImage->data = stbi_load( fileName,
		&( outLoadedImage->width ), &( outLoadedImage->height ),
		&( outLoadedImage->comp ), outLoadedImage->reqComp );
#endif

	if( outLoadedImage->data == NULL ) {
		llog( LOG_INFO, "Unable to load image %s! STB Error: %s", fileName, stbi_failure_reason( ) );
		return -1;
	}

	return 0;
}

// Creates a LoadedImage from the data, decoding the data as it would from a file.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadImageFromMemory( const uint8_t* data, size_t dataSize, int requiredComponents, LoadedImage* outLoadedImage )
{
	if( outLoadedImage == NULL ) {
		llog( LOG_INFO, "Attempting to load an image without a place to store it!" );
		return -1;
	}

	outLoadedImage->reqComp = requiredComponents;
	outLoadedImage->data = stbi_load_from_memory( data, (int)dataSize,
		&( outLoadedImage->width ), &( outLoadedImage->height ), &( outLoadedImage->comp ), outLoadedImage->reqComp );

	if( outLoadedImage->data == NULL ) {
		llog( LOG_INFO, "Unable to load image from meory! STB Error: %s", stbi_failure_reason( ) );
		return -1;
	}

	return 0;
}

// Loads the image at the file name. Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadTexture( const char* fileName, Texture* outTexture )
{
	LoadedImage image = { 0 };
	int returnCode = 0;

	if( outTexture == NULL ) {
		llog( LOG_INFO, "Null outTexture passed for file %s!", fileName );
		goto clean_up;
	}

	if( gfxUtil_LoadImage( fileName, &image ) < 0 ) {
		llog( LOG_INFO, "Unable to load image %s! STB Error: %s", fileName, stbi_failure_reason( ) );
		returnCode = -1;
		goto clean_up;
	}

    // GL_RGBA
	if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, &image, outTexture ) < 0 ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	//SDL_FreeSurface( loadSurface );
	stbi_image_free( image.data );
	return returnCode;
}

// Turns an RGBA bitmap into a texture.  Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_CreateTextureFromRGBABitmap( uint8_t* data, int width, int height, Texture* outTexture )
{
	assert( data != NULL );
	assert( width > 0 );
	assert( height > 0 );

	int returnCode = 0;

	LoadedImage image = { 0 };
	image.data = data;
	image.width = width;
	image.height = height;
	image.reqComp = image.comp = 4;

	if( outTexture == NULL ) {
		llog( LOG_INFO, "Null outTexture passed for bitmap!" );
		goto clean_up;
	}

	if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, &image, outTexture ) < 0 ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	return returnCode;
}

// Turns a single channel bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_CreateTextureFromAlphaBitmap( uint8_t* data, int width, int height, Texture* outTexture )
{
	assert( data != NULL );
	assert( width > 0 );
	assert( height > 0 );

	int returnCode = 0;

	LoadedImage image = { 0 };
	image.data = data;
	image.width = width;
	image.height = height;
	image.reqComp = image.comp = 1;

	if( outTexture == NULL ) {
		llog( LOG_INFO, "Null outTexture passed for bitmap!" );
		goto clean_up;
	}

	TextureFormat texFormat;
#if defined( __IPHONEOS__ ) || defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
	texFormat = TF_ALPHA;
#else
	texFormat = TF_RED;
#endif
	if( !gfxPlatform_CreateTextureFromLoadedImage( texFormat, &image, outTexture ) ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	return returnCode;
}

// Returns whether the SDL_Surface has any pixels that have a transparency that aren't completely clear or solid.
int gfxUtil_SurfaceIsTranslucent( SDL_Surface* surface )
{
	Uint8 r, g, b, a;
	int bpp = surface->format->BytesPerPixel;

	for( int y = 0; y < surface->h; ++y ) {
		for( int x = 0; x < surface->w; ++x ) {
			Uint32 pixel;
			// pitch seems to be in bits, not bytes as the documentation says it should be
			Uint8 *pos = ( ( (Uint8*)surface->pixels ) + ( ( y * ( surface->pitch / 8 ) ) + ( x * bpp ) ) );
			switch( bpp ) {
			case 3:
				if( SDL_BYTEORDER == SDL_BIG_ENDIAN ) {
					pixel = ( pos[0] << 16 ) | ( pos[1] << 8 ) | pos[2];
				} else {
					pixel = pos[0] | ( pos[1] << 8 ) | ( pos[2] << 16 );
				}
				break;
			case 4:
				pixel = *( (Uint32*)pos );
				break;
			default:
				pixel = 0;
				break;
			}
			SDL_GetRGBA( pixel, surface->format, &r, &g, &b, &a );
			if( ( a > 0x00 ) && ( a < 0xFF ) ) {
				return 1;
			}
		}
	}

	return 0;
}

void gfxUtil_TakeScreenShot( const char* outputPath )
{
	int width = 0;
	int height = 0;
	gfx_GetWindowSize( &width, &height );

	// need to make sure you release the memory allocated for the pixels
	uint8_t* pixels = gfxPlatform_GetScreenShotPixels( width, height );
	stbi_write_png( outputPath, width, height, 3, pixels, width * 3 );

	mem_Release( pixels );
}
