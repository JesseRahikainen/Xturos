#include "gfxUtil.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL_log.h>
#include <SDL_endian.h>

/*
Loads the image at the file name. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_LoadTexture( const char* fileName, Texture* outTexture )
{
	int width, height, comp;
	SDL_Surface* loadSurface = NULL;
	unsigned char* imageData = NULL;
	int returnCode = 0;

	if( outTexture == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Null outTexture passed for file %s!", fileName );
		goto clean_up;
	}

	// load and convert file to pixel data
	imageData = stbi_load( fileName, &width, &height, &comp, 4 );
	if( imageData == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to load image %s! STB Error: %s", fileName, stbi_failure_reason( ) );
		returnCode = -1;
		goto clean_up;
	}
	
	int bitsPerPixel = comp * 8;
	loadSurface = SDL_CreateRGBSurfaceFrom( (void*)imageData, width, height, bitsPerPixel, ( width * bitsPerPixel ), 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 );
	if( loadSurface == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to create RGB surface for %s! SDL Error: %s", fileName, SDL_GetError( ) );
		returnCode = -1;
		goto clean_up;
	}

	if( gfxUtil_CreateTextureFromSurface( loadSurface, outTexture ) < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to convert surface to texture for %s! SDL Error: %s", fileName, SDL_GetError( ) );
		returnCode = -1;
		goto clean_up;
	}

clean_up:
	SDL_FreeSurface( loadSurface );
	stbi_image_free( imageData );
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
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to handle format!" );
		return -1;
	}

	glGenTextures( 1, &( outTexture->textureID ) );

	if( outTexture->textureID == 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to create texture object." );
		return -1;
	}

	glBindTexture( GL_TEXTURE_2D, outTexture->textureID );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTexImage2D( GL_TEXTURE_2D, 0, texFormat, surface->w, surface->h, 0, texFormat, GL_UNSIGNED_BYTE, surface->pixels );

	outTexture->width = surface->w;
	outTexture->height = surface->h;
	outTexture->flags = 0;
	if( gfxUtil_SurfaceIsTranslucent( surface ) ) {
		outTexture->flags |= TF_IS_TRANSPARENT;
	}

	return 0;
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