#ifndef IMAGES_H
#define IMAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

#include "gfx_commonDataTypes.h"

#include "Math/vector2.h"
#include "color.h"
#include "triRendering.h"
#include "gfxUtil.h"
#include "Math/matrix3.h"

// Initializes images.
bool img_Init( void );

//************ Threaded functions
// Loads the image in a seperate thread. Puts the resulting image index into outIdx.
//  Also calls the onLoadDone callback with the id for the image, passes in -1 if it fails for any reason
void img_ThreadedLoad( const char* fileName, ShaderType shaderType, ImageID* outIdx, void (*onLoadDone)( ImageID ) );

//************ End threaded functions

// Loads the image stored at file name.
//  Returns the index of the image on success.
//  Returns INVALID_IMAGE_ID on failure, and prints a message to the log.
ImageID img_Load( const char* fileName, ShaderType shaderType );

// Returns whether imgIdx points to a valid image.
bool img_IsValidImage( ImageID imgId );

// Creates an image from a LoadedImage.
ImageID img_CreateFromLoadedImage( LoadedImage* loadedImg, ShaderType shaderType, const char* id );

// Creates an image from a texture.
ImageID img_CreateFromTexture( Texture* texture, ShaderType shaderType, const char* id );

// Creates an image from a surface.
ImageID img_Create( SDL_Surface* surface, ShaderType shaderType, const char* id );

// Cleans up an image at the specified index, trying to render with it after this won't work.
void img_Clean( ImageID id );

// Takes in an already loaded texture and some rectangles. I'ts assume the length of mins, maxes and retIDs equals count.
//  Returns the package ID used to clean up later, returns -1 if there's a problem.
int img_SplitTexture( Texture* texture, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, int packageID, ImageID* retIDs );

// Takes in a file name and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitImageFile( char* fileName, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, char** imgIDs, ImageID* retIDs );

// Takes in an RGBA bitmap and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitRGBABitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, ImageID* retIDs );

// Takes in a one channel bitmap and some rectangles. It's assume the length of the mins, maxes, and retIDs equal scount.
//  Returns package ID used to clean up later, returns -1 if there's a problem.
int img_SplitAlphaBitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, ImageID* retIDs );

// Gets all the images associated with the packageID and returns a stretchy buffer containing them.
ImageID* img_GetPackageImages( int packageID );

// Gets all the number of images associated with the packageID.
size_t img_GetPackageImageCount( int packageID );

// Cleans up all the images in an image package.
void img_CleanPackage( int packageID );

// Sets an offset to render the image from. The default is the center of the image.
void img_SetOffset( ImageID id, Vector2 offset );

// Sets an offset based on a vector with elements in the ranges [0,1], default is <0.5, 0.5>.
void img_SetRatioOffset( ImageID id, Vector2 offsetRatio, Vector2 padding );

void img_GetOffset( ImageID id, Vector2* out );

void img_ForceTransparency( ImageID id, bool transparent );

// Gets the size of the image, putting it into the out Vector2. Returns false if there's an issue.
bool img_GetSize( ImageID id, Vector2* out );

// returns the ShaderType of the image, returns NUM_SHADERS if the image isn't valid
ShaderType img_GetShaderType( ImageID id );

// used to override the ShaderType value used when the image was loaded, helpful for sprite sheets
void img_SetShaderType( ImageID id, ShaderType shaderType );

// Gets the the min and max uv coordinates used by the image.
bool img_GetUVCoordinates( ImageID id, Vector2* outMin, Vector2* outMax );

// Gets a scale to use for the image to get a desired size.
bool img_GetDesiredScale( ImageID id, Vector2 desiredSize, Vector2* outScale );

// Gets the texture id for the image, used if you need to render it directly instead of going through this.
//  Returns whether out was successfully set or not.
bool img_GetTextureID( ImageID id, PlatformTexture* out );

// Retrieves a loaded image by it's id, for images loaded from files this will be the local path, for sprite sheet images 
ImageID img_GetExistingByStrID( const char* id );

// Sets the image at colorIdx to use use alphaIdx as a signed distance field alpha map
bool img_SetSDFAlphaMap( ImageID colorId, ImageID alphaId );

// Get the id of the first valid image. Returns -1 if there are none.
ImageID img_FirstValidID( void );

// Gets the next valid image after the one passed in. Returns -1 if there are none.
ImageID img_NextValidID( ImageID id );

// Get the human readable id for the image.
const char* img_GetImgStringID( ImageID imgID );

// **************************************************************************************************************************************************************

typedef struct {
	ImageID imgID;
	uint32_t camFlags;
	int8_t depth;
	Matrix3 mat;
	Color color;
	float val0;
	bool isStencil;
	int8_t stencilID;

	// stores the part of the image we actually want to display as normalized positions
	Vector2 subSectionTopLeftNorm;
	Vector2 subSectionBottomRightNorm;

} ImageRenderInstruction;

ImageRenderInstruction img_CreateDefaultRenderInstruction( void );
void img_ImmediateRender( ImageRenderInstruction* instruction );

// returns if it was able to set the borders correctly
bool img_SetRenderInstructionBorders( ImageRenderInstruction* instruction, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom );

// helpers so you don't have to worry about creating an instruction, filling it out, and then calling immediate render
void img_Render_Pos( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos );
void img_Render_PosClr( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, const Color* clr );
void img_Render_PosRot( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad );
void img_Render_PosRotClr( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad, const Color* clr );
void img_Render_PosScaleVClr( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, const Vector2* scale, const Color* clr );
void img_Render_PosSizeVClr( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, const Vector2* size, const Color* clr );
void img_Render_PosRotScaleClr( ImageID imgID, uint32_t camFlags, int8_t depth, const Vector2* pos, float rotRad, float scale, const Color* clr );

#endif /* inclusion guard */