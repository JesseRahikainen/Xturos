#ifndef IMAGES_H
#define IMAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "Math/vector2.h"
#include "color.h"
#include "triRendering.h"
#include "gfxUtil.h"

// Initializes images.
//  Returns < 0 on an error.
int img_Init( void );

//************ Threaded functions
// Loads the image in a seperate thread. Puts the resulting image index into outIdx.
//  Also calls the onLoadDone callback with the id for the image, passes in -1 if it fails for any reason
void img_ThreadedLoad( const char* fileName, ShaderType shaderType, int* outIdx, void (*onLoadDone)( int ) );

//************ End threaded functions

// Loads the image stored at file name.
//  Returns the index of the image on success.
//  Returns -1 on failure, and prints a message to the log.
int img_Load( const char* fileName, ShaderType shaderType );

// Creates an image from a LoadedImage.
int img_CreateFromLoadedImage( LoadedImage* loadedImg, ShaderType shaderType, const char* id );

// Creates an image from a texture.
int img_CreateFromTexture( Texture* texture, ShaderType shaderType, const char* id );

// Creates an image from a surface.
int img_Create( SDL_Surface* surface, ShaderType shaderType, const char* id );

// Cleans up an image at the specified index, trying to render with it after this won't work.
void img_Clean( int idx );

// Takes in an already loaded texture and some rectangles. I'ts assume the length of mins, maxes and retIDs equals count.
//  Returns the package ID used to clean up later, returns -1 if there's a problem.
int img_SplitTexture( Texture* texture, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, int* retIDs );

// Takes in a file name and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitImageFile( char* fileName, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, int* retIDs );

// Takes in an RGBA bitmap and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitRGBABitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs );

// Takes in a one channel bitmap and some rectangles. It's assume the length of the mins, maxes, and retIDs equal scount.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitAlphaBitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs );

// Cleans up all the images in an image package.
void img_CleanPackage( int packageID );

// Sets an offset to render the image from. The default is the center of the image.
void img_SetOffset( int idx, Vector2 offset );

// Sets an offset based on a vector with elements in the ranges [0,1], default is <0.5, 0.5>.
void img_SetRatioOffset( int idx, Vector2 offsetRatio, Vector2 padding );

void img_ForceTransparency( int idx, bool transparent );

// Gets the size of the image, putting it into the out Vector2. Returns a negative number if there's an issue.
int img_GetSize( int idx, Vector2* out );

// Gets a scale to use for the image to get a desired size.
int img_GetDesiredScale( int idx, Vector2 desiredSize, Vector2* outScale );

// Gets the texture id for the image, used if you need to render it directly instead of going through this.
//  Returns whether out was successfully set or not.
int img_GetTextureID( int idx, PlatformTexture* out );

// Retrieves a loaded image by it's id, for images loaded from files this will be the local path, for sprite sheet images 
int img_GetExistingByID( const char* id );

// Sets the image at colorIdx to use use alphaIdx as a signed distance field alpha map
int img_SetSDFAlphaMap( int colorIdx, int alphaIdx );

// returns a draw id that is passed to the img_Set* functions to set the various values used for the draw
//  NOTE: Calling the scales more than once will apply the scales, not set the scale to the passed in value
int img_CreateDraw( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth );
void img_SetDrawScale( int drawID, float start, float end );
void img_SetDrawScaleV( int drawID, Vector2 start, Vector2 end );
void img_SetDrawColor( int drawID, Color start, Color end );
void img_SetDrawRotation( int drawID, float start, float end );
void img_SetDrawFloatVal0( int drawID, float start, float end );
void img_SetDrawStencil( int drawID, bool isStencil, int stencilID );
void img_SetDrawSize( int drawID, Vector2 start, Vector2 end ); // overrides scaling

// all 3x3 draw from the center
//  TODO: Get this switched over to the using the same system as the images now use. Will require some more work.
int img_Draw3x3( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth );
int img_Draw3x3v( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth );

int img_Draw3x3_c_f( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize,
	Color startColor, Color endColor, float startVal0, float endVal0, int8_t depth );
int img_Draw3x3v_c( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos,
	Vector2 startSize, Vector2 endSize, Color startColor, Color endColor, int8_t depth );
int img_Draw3x3v_c_f( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos,
	Vector2 startSize, Vector2 endSize, Color startColor, Color endColor, float startVal0, float endVal0, int8_t depth );

// Clears the image draw list.
void img_ClearDrawInstructions( void );

// Draw all the images.
void img_Render( float normTimeElapsed );

#endif /* inclusion guard */