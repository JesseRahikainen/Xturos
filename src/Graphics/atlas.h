#ifndef ATLAS_H
#define ATLAS_H

// resource management
// general algorithm will be quite simple, just sort by total size, find the largest that fits, add it and continue
//  but how do we deal with the packing failing, there isn't enough room left on the atlas? have some sort of id
//  that is based back (atlasImage) and it's -1 if it fails to pack that image
int createAtlas( int atlas, int numImages, void** imagesData, int* imagesWidths, int* imagesHeights, int* outAtlasImageIDs );
void destroyAtlas( int atlas );

// rendering
void bindAtlasForRender( int atlas );

#endif // inclusion guard