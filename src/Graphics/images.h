#ifndef IMAGES_H
#define IMAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "../Math/vector2.h"
#include "color.h"
#include "triRendering.h"

/*
Initializes images.
 Returns < 0 on an error.
*/
int img_Init( void );

//************ Threaded functions
/*
Loads the image in a seperate thread. Puts the resulting image index into outIdx.
*/
void img_ThreadedLoad( const char* fileName, ShaderType shaderType, int* outIdx );

//************ End threaded functions

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int img_Load( const char* fileName, ShaderType shaderType );

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface, ShaderType shaderType );

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void img_Clean( int idx );

/*
Takes in a file name and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitImageFile( char* fileName, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs );

/*
Takes in an RGBA bitmap and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitRGBABitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs );

/*
Takes in a one channel bitmap and some rectangles. It's assume the length of the mins, maxes, and retIDs equal scount.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitAlphaBitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs );

/*
Cleans up all the images in an image package.
*/
void img_CleanPackage( int packageID );

/*
Sets an offset to render the image from. The default is the center of the image.
*/
void img_SetOffset( int idx, Vector2 offset );

/*
Gets the size of the image, putting it into the out Vector2. Returns a negative number if there's an issue.
*/
int img_GetSize( int idx, Vector2* out );

/*
Gets the texture id for the image, used if you need to render it directly instead of going through this.
 Returns whether out was successfully set or not.
*/
int img_GetTextureID( int idx, GLuint* out );

/*
Adds to the list of images to draw.
*/
int img_Draw( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth );
int img_Draw_s( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, int8_t depth );
int img_Draw_sv( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, int8_t depth );
int img_Draw_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, int8_t depth );
int img_Draw_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startRotRad, float endRotRad, int8_t depth );
int img_Draw_s_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, int8_t depth );
int img_Draw_s_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRotRad, float endRotRad, int8_t depth );
int img_Draw_sv_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, int8_t depth );
int img_Draw_sv_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRotRad, float endRotRad, int8_t depth );
int img_Draw_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor,
	float startRotRad, float endRotRad, int8_t depth );
int img_Draw_s_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, float startRotRad, float endRotRad, int8_t depth );
int img_Draw_sv_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, float startRotRad, float endRotRad, int8_t depth );

// all 3x3 draw from the center
int img_Draw3x3( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth );
int img_Draw3x3v( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth );

int img_Draw3x3_c( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize,
	Color startColor, Color endColor, int8_t depth );
int img_Draw3x3v_c( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos,
	Vector2 startSize, Vector2 endSize, Color startColor, Color endColor, int8_t depth );

/*
Clears the image draw list.
*/
void img_ClearDrawInstructions( void );

/*
Draw all the images.
*/
void img_Render( float normTimeElapsed );

#endif /* inclusion guard */