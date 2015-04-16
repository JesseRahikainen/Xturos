#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include <SDL.h>
#include "../Math/vector2.h"
#include "color.h"

/*  ======= Package Loading ======= */
/*
Takes in a list of file names and loads them together. It's assumed the length of fileNames and retIDs equals count.
 Returns the package ID used to clean up later, returns -1 if there's a problem.
*/
int loadImagePackage( int count, char** fileNames, int* retIDs );

/*
Cleans up all the images in an image package.
*/
void cleanPackage( int packageID );

/*  ======= Image loading ======= */
/*
Initializes the stored images as well as the image loading.
 Returns >=0 on success.
*/
int initImages( void );

/*
Clean up all the images.
*/
void cleanImages( void );

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int loadImage( const char* fileName );

/*
Creates an image from a surface.
*/
int createImage( SDL_Surface* surface );

/*
Creates an image from a string, used for rendering text. Can be passed in
 an existing image to overwrite it's spot, send a negative number otherwise.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int createNewTextImage( const char* text, const char* fontName, int pointSize, SDL_Color color, Uint32 wrapLen );
int createTextImage( const char* text, const char* fontName, int pointSize, SDL_Color color, Uint32 wrapLen, int existingImage );

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void cleanImageAt( int idx );

/*
Sets an offset to render the image from.
*/
void setImageOffset( int idx, Vector2 offset );

/* ======= Rendering ======= */
/*
Initial setup for the rendering instruction buffer.
 Returns a negative number on failure.
*/
int initRendering( SDL_Window* window );

/*
Sets clearing color.
*/
void setRendererClearColor( float red, float green, float blue, float alpha );

/*
Adds a render instruction to the buffer.
 Returns 0 on success. Prints an error message to the log if it fails.
*/
int queueRenderImage( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth );
int queueRenderImage_s( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, char depth );
int queueRenderImage_sv( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, char depth );
int queueRenderImage_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, char depth );
int queueRenderImage_s_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, char depth );
int queueRenderImage_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot, char depth );
int queueRenderImage_s_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRot, float endRot, char depth );
int queueRenderImage_sv_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, char depth );
int queueRenderImage_r_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot,
	Color startColor, Color endColor, char depth );
int queueRenderImage_sv_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, Color startColor, Color endColor, char depth );

/*
Some basic debug drawing functions.
  Returns 0 on success. Prints an error message to the log if it fails and returns -1.
*/
int queueDebugDraw_AABB( unsigned int camFlags, Vector2 topLeft, Vector2 size, float red, float green, float blue );
int queueDebugDraw_Line( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, float red, float green, float blue );
int queueDebugDraw_Circle( unsigned int camFlags, Vector2 center, float radius, float red, float green, float blue );

/*
Clears the render queue.
*/
void clearRenderQueue( float endTime );

/*
Goes through everything in the render buffer and does the actual rendering.
Resets the render buffer.
*/
void renderImages( float deltaTime );

#endif
