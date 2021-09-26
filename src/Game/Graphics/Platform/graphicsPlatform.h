#ifndef GRAPHICS_PLATFORM
#define GRAPHICS_PLATFORM

#include <SDL_video.h>
#include <stdbool.h>

#include "Graphics/color.h"
#include "Graphics/gfxUtil.h"

// this header acts as an interface for the platform specific graphic functions

bool gfxPlatform_Init( SDL_Window* window, int desiredRenderWidth, int desiredRenderHeight );

int gfxPlatform_GetMaxSize( int desiredSize );

void gfxPlatform_DynamicSizeRender( float dt, float t, int renderWidth, int renderHeight,
	int windowRenderX0, int windowRenderY0, int windowRenderX1, int windowRenderY1, Color clearColor );

void gfxPlatform_StaticSizeRender( float dt, float t, Color clearColor );

void gfxPlatform_CleanUp( void );

void gfxPlatform_ShutDown( void );

bool gfxPlatform_CreateTextureFromLoadedImage( GLenum texFormat, LoadedImage* image, Texture* outTexture );

bool gfxPlatform_CreateTextureFromSurface( SDL_Surface* surface, Texture* outTexture );

void gfxPlatform_UnloadTexture( Texture* texture );

int gfxPlatform_ComparePlatformTextures( PlatformTexture rhs, PlatformTexture lhs );

void gfxPlatform_DeletePlatformTexture( PlatformTexture texture );

uint8_t* gfxPlatform_GetScreenShotPixels( int width, int height );

#endif // inclusion guard