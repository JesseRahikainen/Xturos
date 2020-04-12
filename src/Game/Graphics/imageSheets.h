#ifndef IMAGE_SHEETS_H
#define IMAGE_SHEETS_H

#include "triRendering.h"

/*
This opens up the sprite sheet file and loads all the images, putting image ids into imgOutArray. The returned array
 uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
 the size of it.
 The returned integers are ids for the images source files.
 Returns the number of images loaded if it was successful, otherwise returns -1.
*/
int img_LoadSpriteSheet( char* fileName, ShaderType shaderType, int** imgOutArray );

/*
Cleans up all the images created from img_LoadSpriteSheet( ). The pointer passed in will be invalid after this
 is called.
*/
void img_UnloadSpriteSheet( int* imgArray );

#endif /* inclusion guard */