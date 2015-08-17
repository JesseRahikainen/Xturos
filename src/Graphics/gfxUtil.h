#ifndef GFX_UTIL_H
#define GFX_UTIL_H

#include "../Others/glew.h"
#include "../Math/vector2.h"
#include <SDL_surface.h>

// some basic texture handling things.
enum TextureFlags {
	TF_IS_TRANSPARENT = 0x1
};

typedef struct {
	GLuint textureID;
	int width;
	int height;
	int flags;
} Texture;

typedef struct {
	Vector2 minUV;
	Vector2 maxUV;
	Texture texture;
} AtlasResult;

/*
Loads the image at the file name. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_LoadTexture( const char* fileName, Texture* outTexture );

/*
Turns an SDL_Surface into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_CreateTextureFromSurface( SDL_Surface* surface, Texture* outTexture );

/*
Turns an RGBA bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_CreateTextureFromRGBABitmap( uint8_t* data, int width, int height, Texture* outTexture );

/*
Turns a single channel bitmap into a texture. Takes in a pointer to a Texture structure that it puts all the generated data into.
 Returns >= 0 on success, < 0 on failure.
*/
int gfxUtil_CreateTextureFromAlphaBitmap( uint8_t* data, int width, int height, Texture* outTexture );

/*
Unloads the passed in texture. After doing this the texture will be invalid and should not be used anymore.
*/
void gfxUtil_UnloadTexture( Texture* texture );

/*
Returns whether the SDL_Surface has any pixels that have a transparency that aren't completely clear or solid.
*/
int gfxUtil_SurfaceIsTranslucent( SDL_Surface* surface );

#endif /* inclusion guard */