#ifndef IMAGE_SHEETS_H
#define IMAGE_SHEETS_H

#include "triRendering.h"
#include "Graphics/gfx_commonDataTypes.h"

typedef struct {
	char* sbPath;
} SpriteSheetEntry;

// This opens up the sprite sheet file and loads all the images, putting image ids into sbImgOutArray. The returned array
//  uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
//  the size of it.
// The returned integers are ids for the images source files.
// Returns the package id for the sprite sheet if it was successful, otherwise returns -1.
int img_LoadSpriteSheet( const char* fileName, ShaderType shaderType, ImageID** sbImgOutArray );

// Decrements and checks the loaded count for the sprite sheet that uses this package, if it's
//  <= 0 it unloads all the images associated with the package.
void img_UnloadSpriteSheet( int packageID );

// Takes in a list of file names and generates the sprite sheet and saves it out to fileName.
bool img_SaveSpriteSheet( const char* fileName, SpriteSheetEntry* sbEntries, int maxSize, int xPadding, int yPadding );

void img_ThreadedLoadSpriteSheet( const char* fileName, ShaderType shaderType, void ( *onLoadDone )( int ) );

#endif /* inclusion guard */