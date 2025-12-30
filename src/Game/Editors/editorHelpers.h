#ifndef EDITOR_HELPERS_H
#define EDITOR_HELPERS_H

#include <stdbool.h>
#include "Graphics/images.h"

// returns the image id loaded
int editor_loadImageFile( const char* filePath );

// returns a stretchybuffer of image ids, you'll need to manage the memory here
int editor_loadSpriteSheetFile( const char* filePath, ImageID** outImgSB );

void editor_chooseLoadFileLocation( const char* fileTypeDesc, const char* fileExtension, bool multiSelect, void (*callback)(const char*) );
void editor_chooseSaveFileLocation( const char* fileTypeDesc, const char* fileExtension, void ( *callback )( const char* ) );

#endif // inclusion guard