#ifndef GFX_UTIL_H
#define GFX_UTIL_H

#include <stdint.h>
#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Math/vector2.h"
#include <SDL_surface.h>

#if defined( OPENGL_GFX )
	#include "Graphics/Platform/OpenGL/graphicsDataTypes_OpenGL.h"
#elif defined( METAL_GFX )
    #include "Graphics/Platform/Metal/graphicsDataTypes_Metal.h"
#else
	#warning "NO DATA TYPES FOR THIS GRAPHICS PLATFORM!"
#endif

// some basic texture handling things.
enum TextureFlags {
	TF_IS_TRANSPARENT = 0x1
};

typedef struct {
	PlatformTexture texture;
	int width;
	int height;
	int flags;
} Texture;

typedef struct {
	Vector2 minUV;
	Vector2 maxUV;
	Texture texture;
} AtlasResult;

typedef struct {
	uint8_t* data;
	int width, height, reqComp, comp;
} LoadedImage;

// Clean up anything that was created in a loaded image.
void gfxUtil_ReleaseLoadedImage( LoadedImage* image );

// Loads the data from fileName into outLoadedImage, used as an intermediary between loading and creating a texture.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadImage( const char* fileName, LoadedImage* outLoadedImage );

// Creates a LoadedImage from the data, decoding the data as it would from a file.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadImageFromMemory( const uint8_t* data, size_t dataSize, int requiredComponents, LoadedImage* outLoadedImage );;

// Loads the image at the file name. Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_LoadTexture( const char* fileName, Texture* outTexture );

// Turns an RGBA bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_CreateTextureFromRGBABitmap( uint8_t* data, int width, int height, Texture* outTexture );

// Turns a single channel bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
//  Returns >= 0 on success, < 0 on failure.
int gfxUtil_CreateTextureFromAlphaBitmap( uint8_t* data, int width, int height, Texture* outTexture );

// Returns whether the SDL_Surface has any pixels that have a transparency that aren't completely clear or solid.
int gfxUtil_SurfaceIsTranslucent( SDL_Surface* surface );

// Takes a screenshot of the current window and puts it at outputPath.
void gfxUtil_TakeScreenShot( const char* outputPath );

#endif /* inclusion guard */