#include "gfxUtil.h"

#include <SDL_endian.h>


// gets sbt_image.h to use our custom memory allocation
#include <stdlib.h>
#include "../System/memory.h"
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

#include "glDebugging.h"

#include "../System/platformLog.h"

/*
Clean up anything that was created in a loaded image.
*/
void gfxUtil_ReleaseLoadedImage( LoadedImage* image )
{
	assert( image != NULL );

	stbi_image_free( image->data );
}

/*
Converts the LoadedImage into a texture, putting everything in outTexture. All LoadedImages are assumed to be in RGBA format.
 Returns >= 0 if everything went fine, < 0 if something went wrong.
*/
int gfxUtil_CreateTextureFromLoadedImage( GLenum texFormat, LoadedImage* image, Texture* outTexture )
{
	//GLenum texFormat = GL_RGBA;

	GL( glGenTextures( 1, &( outTexture->textureID ) ) );

	if( outTexture->textureID == 0 ) {
		llog( LOG_INFO, "Unable to create texture object." );
		return -1;
	}

	GL( glBindTexture( GL_TEXTURE_2D, outTexture->textureID ) );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );

	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

	GL( glTexImage2D( GL_TEXTURE_2D, 0, texFormat, image->width, image->height, 0, texFormat, GL_UNSIGNED_BYTE, image->data ) );

	outTexture->width = image->width;
	outTexture->height = image->height;
	outTexture->flags = 0;

	// check to see if there are any translucent pixels in the image
	for( int i = 3; ( i < ( image->width * image->height ) ) && !( outTexture->flags & TF_IS_TRANSPARENT ); i += 4 ) {
		if( ( image->data[i] > 0x00 ) && ( image->data[i] < 0xFF ) ) {
			outTexture->flags |= TF_IS_TRANSPARENT;
		}
	}

	return 0;
}

/*
 Loads the data from fileName into outLoadedImage, used as an intermediary between loading and creating
  a texture.
  Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_LoadImage( const char* fileName, LoadedImage* outLoadedImage )
{
	if( outLoadedImage == NULL ) {
		llog( LOG_INFO, "Attempting to load an image without a place to store it! %s", fileName );
		return -1;
	}

	outLoadedImage->reqComp = 4;

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

/*
Loads the image at the file name. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
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

	if( gfxUtil_CreateTextureFromLoadedImage( GL_RGBA, &image, outTexture ) < 0 ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	//SDL_FreeSurface( loadSurface );
	stbi_image_free( image.data );
	return returnCode;
}

/*
Turns an SDL_Surface into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_CreateTextureFromSurface( SDL_Surface* surface, Texture* outTexture )
{
	// convert the pixels into a texture
	GLenum texFormat;

	if( surface->format->BytesPerPixel == 4 ) {
		texFormat = GL_RGBA;
	} else if( surface->format->BytesPerPixel == 3 ) {
		texFormat = GL_RGB;
	} else {
		llog( LOG_INFO, "Unable to handle format!" );
		return -1;
	}

	// TODO: Get this working with createTextureFromLoadedImage( ), just need to make an SDL_Surface into a LoadedImage
	glGenTextures( 1, &( outTexture->textureID ) );

	if( outTexture->textureID == 0 ) {
		llog( LOG_INFO, "Unable to create texture object." );
		return -1;
	}

	glBindTexture( GL_TEXTURE_2D, outTexture->textureID );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTexImage2D( GL_TEXTURE_2D, 0, texFormat, surface->w, surface->h, 0, texFormat, GL_UNSIGNED_BYTE, surface->pixels );

	outTexture->width = surface->w;
	outTexture->height = surface->h;
	outTexture->flags = 0;
	// the reason this isn't using the createTextureFromLoadedImage is this primarily
	if( gfxUtil_SurfaceIsTranslucent( surface ) ) {
		outTexture->flags |= TF_IS_TRANSPARENT;
	}

	return 0;
}

/*
Turns an RGBA bitmap into a texture.  Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
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

	if( gfxUtil_CreateTextureFromLoadedImage( GL_RGBA, &image, outTexture ) < 0 ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	return returnCode;
}

/*
Turns a single channel bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
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

	GLenum texFormat;
#if defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
	texFormat = GL_ALPHA;
#else
	texFormat = GL_RED;
#endif
	if( gfxUtil_CreateTextureFromLoadedImage( texFormat, &image, outTexture ) < 0 ) {
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	return returnCode;
}

/*
Unloads the passed in texture. After doing this the texture will be invalid and should not be used anymore.
*/
void gfxUtil_UnloadTexture( Texture* texture )
{
	glDeleteTextures( 1, &( texture->textureID ) );
	texture->textureID = 0;
	texture->flags = 0;
}

/*
Returns whether the SDL_Surface has any pixels that have a transparency that aren't completely clear or solid.
*/
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