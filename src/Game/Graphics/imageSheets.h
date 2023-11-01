#ifndef IMAGE_SHEETS_H
#define IMAGE_SHEETS_H

#include "triRendering.h"

typedef struct {
	char* sbPath;
} SpriteSheetEntry;

// This opens up the sprite sheet file and loads all the images, putting image ids into imgOutArray. The returned array
//  uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
//  the size of it.
// The returned integers are ids for the images source files.
// Returns the number of images loaded if it was successful, otherwise returns -1.
int img_LoadSpriteSheet( const char* fileName, ShaderType shaderType, int** imgOutArray );

// Takes in a list of file names and generates the sprite sheet and saves it out to fileName.
bool img_SaveSpriteSheet( const char* fileName, SpriteSheetEntry* sbEntries, int maxSize, int xPadding, int yPadding );

// Takes in an image and the dividing lines and creates a 3x3 sprite sheet for it.
bool img_Save3x3( const char* fileName, const char* imageFileName, int width, int height, int left, int right, int top, int bottom );

// Cleans up all the images created from img_LoadSpriteSheet( ). The pointer passed in will be invalid after this is called.
void img_UnloadSpriteSheet( int* imgArray );

void img_ThreadedLoadSpriteSheet( const char* fileName, ShaderType shaderType, void ( *onLoadDone )( int ) );

#endif /* inclusion guard */