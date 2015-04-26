#ifndef IMAGES_H
#define IMAGES_H

#include <SDL.h>
#include "../Math/vector2.h"
#include "color.h"

/*
Initializes images.
 Returns < 0 on an error.
*/
int img_Init( void );

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int img_Load( const char* fileName );

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface );

/*
Creates an image from a string, used for rendering text.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int img_CreateText( const char* text, const char* fontName, int pointSize, Color color, Uint32 wrapLen );

/*
Changes the existing image instead of trying to create a new one.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int img_UpdateText( const char* text, const char* fontName, int pointSize, Color color, Uint32 wrapLen, int existingImage );

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void img_Clean( int idx );

/*
Takes in a list of file names and loads them together. It's assumed the length of fileNames and retIDs equals count.
 Returns the package ID used to clean up later, returns -1 if there's a problem.
*/
int img_LoadPackage( int count, char** fileNames, int* retIDs );

/*
Cleans up all the images in an image package.
*/
void img_CleanPackage( int packageID );

/*
Sets an offset to render the image from. The default is the center of the image.
*/
void img_SetOffset( int idx, Vector2 offset );

/*
Adds to the list of images to draw.
*/
int img_Draw( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth );
int img_Draw_s( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, char depth );
int img_Draw_sv( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, char depth );
int img_Draw_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, char depth );
int img_Draw_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot, char depth );
int img_Draw_s_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, char depth );
int img_Draw_s_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRot, float endRot, char depth );
int img_Draw_sv_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, char depth );
int img_Draw_sv_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, char depth );
int img_Draw_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor,
	float startRot, float endRot, char depth );
int img_Draw_s_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, float startRot, float endRot, char depth );
int img_Draw_sv_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, float startRot, float endRot, char depth );

/*
Clears the image draw list.
*/
void img_ClearDrawInstructions( void );

/*
Draw all the images.
*/
void img_Render( float normTimeElapsed );

#endif /* inclusion guard */