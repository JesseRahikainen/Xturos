#include "images.h"

#include "../Graphics/glPlatform.h"
#include "glPlatform.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "../Math/matrix4.h"
#include "gfxUtil.h"
#include "scissor.h"
#include "../System/platformLog.h"
#include "../Math/mathUtil.h"

/* Image loading types and variables */
#define MAX_IMAGES 512

enum {
	IMGFLAG_IN_USE = 0x1,
	IMGFLAG_HAS_TRANSPARENCY = 0x2,
};

typedef struct {
	GLuint textureObj;
	Vector2 uvMin;
	Vector2 uvMax;
	Vector2 size;
	Vector2 offset;
	int flags;
	int packageID;
	int nextInPackage;
	ShaderType shaderType;
} Image;

static Image images[MAX_IMAGES];

/* Rendering types and variables */
#define MAX_RENDER_INSTRUCTIONS 1024
typedef struct {
	Vector2 pos;
	Vector2 scaleSize;
	Color color;
	float rotation;
} DrawInstructionState;

typedef struct {
	GLuint textureObj;
	int imageObj;
	Vector2 offset;
	Vector2 uvs[4];
	DrawInstructionState start;
	DrawInstructionState end;
	int flags;
	int scissorID;
	uint32_t camFlags;
	int8_t depth;
	ShaderType shaderType;
} DrawInstruction;

static DrawInstruction renderBuffer[MAX_RENDER_INSTRUCTIONS];
static int lastDrawInstruction;

static const DrawInstruction DEFAULT_DRAW_INSTRUCTION = {
	0, -1, { 0.0f, 0.0f },
	{ { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f },
	0, 0, 0
};

static GLint maxTextureSize;

/*
Initializes images.
 Returns < 0 on an error.
*/
int img_Init( void )
{
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );
	memset( images, 0, sizeof(images) );
	return 0;
}

/*
Finds the first unused image index.
 Returns a postive value on success, a negative on failure.
*/
static int findAvailableImageIndex( )
{
	int newIdx = 0;

	while( ( newIdx < MAX_IMAGES ) && ( images[newIdx].flags & IMGFLAG_IN_USE ) ) {
		++newIdx;
	}

	if( newIdx >= MAX_IMAGES ) {
		newIdx = -1;
	}

	return newIdx;
}

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int img_Load( const char* fileName, ShaderType shaderType )
{
	int newIdx = -1;

	// find the first empty spot, make sure we won't go over our maximum
	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to load image %s! Image storage full.", fileName );
		return -1;
	}

	Texture texture;
	if( gfxUtil_LoadTexture( fileName, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to load image %s!", fileName );
		newIdx = -1;
		return -1;
	}

	images[newIdx].textureObj = texture.textureID;
	images[newIdx].size.v[0] = (float)texture.width;
	images[newIdx].size.v[1] = (float)texture.height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	images[newIdx].nextInPackage = -1;
	images[newIdx].uvMin = VEC2_ZERO;
	images[newIdx].uvMax = VEC2_ONE;
	images[newIdx].shaderType = shaderType;
	if( texture.flags & TF_IS_TRANSPARENT ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	return newIdx;
}

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface, ShaderType shaderType )
{
	int newIdx;

	assert( surface != NULL );

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		llog( LOG_INFO, "Unable to create image from surface! Image storage full." );
		return -1;
	}

	Texture texture;
	if( gfxUtil_CreateTextureFromSurface( surface, &texture ) < 0 ) {
		llog( LOG_INFO, "Unable to convert surface to texture! SDL Error: %s", SDL_GetError( ) );
		return -1;
	} else {
		images[newIdx].size.v[0] = (float)texture.width;
		images[newIdx].size.v[1] = (float)texture.height;
		images[newIdx].offset = VEC2_ZERO;
		images[newIdx].packageID = -1;
		images[newIdx].flags = IMGFLAG_IN_USE;
		images[newIdx].nextInPackage = -1;
		images[newIdx].uvMin = VEC2_ZERO;
		images[newIdx].uvMax = VEC2_ONE;
		images[newIdx].shaderType = shaderType;
		if( texture.flags & TF_IS_TRANSPARENT ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}
	}

	return newIdx;
}

/*
Cleans up an image at the specified index, trying to render with it after this won't work.
*/
void img_Clean( int idx )
{
	assert( idx < MAX_IMAGES );
	assert( idx >= 0 );
	assert( images[idx].flags & IMGFLAG_IN_USE );

	int bufIdx;

	if( ( idx < 0 ) || ( ( images[idx].size.v[0] == 0.0f ) && ( images[idx].size.v[1] == 0.0f ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	/* clean up anything we're wanting to draw */
	for( bufIdx = 0; bufIdx <= lastDrawInstruction; ++bufIdx ) {
		if( renderBuffer[bufIdx].imageObj == idx ) {
			if( lastDrawInstruction > 0 ) {
				renderBuffer[bufIdx] = renderBuffer[lastDrawInstruction-1];
			}
			--lastDrawInstruction;
			--bufIdx;
		}
	}

	// see if this is the last image using that texture
	//  TODO: See if this needs to be sped up
	int deleteTexture = 1;
	for( int i = 0; ( i < MAX_IMAGES ) && deleteTexture; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].textureObj == images[idx].textureObj ) ) {
			deleteTexture = 0;
		}
	}

	if( deleteTexture ) {
		glDeleteTextures( 1, &( images[idx].textureObj ) );
	}
	images[idx].size = VEC2_ZERO;
	images[idx].flags = 0;
	images[idx].packageID = -1;
	images[idx].nextInPackage = -1;
	images[idx].uvMin = VEC2_ZERO;
	images[idx].uvMax = VEC2_ZERO;
	images[idx].shaderType = ST_DEFAULT;
}

/*
Finds the next unused package ID.
*/
int findUnusedPackage( void )
{
	int packageID = 0;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID <= packageID ) ) {
			packageID = images[i].packageID + 1;
		}
	}
	return packageID;
}

/*
Splits the texture. Returns a negative number if there's a problem.
*/
int split( Texture* texture, int packageID, ShaderType shaderType, int count, Vector2* mins, Vector2* maxes, int* retIDs )
{
	Vector2 inverseSize;
	inverseSize.x = 1.0f / (float)texture->width;
	inverseSize.y = 1.0f / (float)texture->height;

	for( int i = 0; i < count; ++i ) {
		int newIdx = findAvailableImageIndex( );
		if( newIdx < 0 ) {
			llog( LOG_ERROR, "Problem finding available image to split into." );
			img_CleanPackage( packageID );
			return -1;
		}

		images[newIdx].textureObj = texture->textureID;
		vec2_Subtract( &( maxes[i] ), &( mins[i] ), &( images[newIdx].size ) );
		images[newIdx].offset = VEC2_ZERO;
		images[newIdx].packageID = packageID;
		images[newIdx].flags = IMGFLAG_IN_USE;
		vec2_HadamardProd( &( mins[i] ), &inverseSize, &( images[newIdx].uvMin ) );
		vec2_HadamardProd( &( maxes[i] ), &inverseSize, &( images[newIdx].uvMax ) );
		images[newIdx].shaderType = shaderType;
		if( texture->flags & TF_IS_TRANSPARENT ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}

		retIDs[i] = newIdx;
	}

	return 0;
}

/*
Takes in a file name and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitImageFile( char* fileName, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_LoadTexture( fileName, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem loading image %s", fileName );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, retIDs ) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Takes in an RGBA bitmap and some rectangles. It's assumed the length of mins, maxes, and retIDs equals count.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitRGBABitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_CreateTextureFromRGBABitmap( data, width, height, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem creating RGBA bitmap texture." );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, retIDs ) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Takes in a one channel bitmap and some rectangles. It's assume the length of the mins, maxes, and retIDs equal scount.
 Returns package ID used to clean up later, returns -1 if there's a problem.
*/
int img_SplitAlphaBitmap( uint8_t* data, int width, int height, int count, ShaderType shaderType, Vector2* mins, Vector2* maxes, int* retIDs )
{
	int currPackageID = findUnusedPackage( );

	Texture texture;
	if( gfxUtil_CreateTextureFromAlphaBitmap( data, width, height, &texture ) < 0 ) {
		llog( LOG_ERROR, "Problem creating alpha bitmap texture." );
		return -1;
	}

	if( split( &texture, currPackageID, shaderType, count, mins, maxes, retIDs ) < 0 ) {
		return -1;
	}

	return currPackageID;
}

/*
Cleans up all the images in an image package.
*/
void img_CleanPackage( int packageID )
{
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID == packageID ) ) {
			img_Clean( i );
		}
	}
}

/*
Sets an offset to render the image from. The default is the center of the image.
*/
void img_SetOffset( int idx, Vector2 offset )
{
	assert( idx < MAX_IMAGES );
	assert( idx >= 0 );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	images[idx].offset = offset;
}

/*
Gets the size of the image, putting it into the out Vector2. Returns a negative number if there's an issue.
*/
int img_GetSize( int idx, Vector2* out )
{
	assert( out != NULL );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return -1;
	}

	(*out) = images[idx].size;
	return 0;
}

/*
initializes the structure so only what's used needs to be set, also fill in
  some stuff that all of the queueRenderImage functions use, returns the pointer
  for further setting of other stuff
 returns NULL if there's a problem
*/
static DrawInstruction* GetNextRenderInstruction( int imgObj, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth )
{
	if( !( images[imgObj].flags & IMGFLAG_IN_USE ) ) {
		llog( LOG_VERBOSE, "Attempting to draw invalid image: %i", imgObj );
		return NULL;
	}

	if( lastDrawInstruction >= MAX_RENDER_INSTRUCTIONS ) {
		llog( LOG_VERBOSE, "Render instruction queue full." );
		return NULL;
	}

	++lastDrawInstruction;
	DrawInstruction* ri = &( renderBuffer[lastDrawInstruction] );

	*ri = DEFAULT_DRAW_INSTRUCTION;
	ri->textureObj = images[imgObj].textureObj;
	ri->imageObj = imgObj;
	ri->start.pos = startPos;
	ri->end.pos = endPos;
	ri->start.scaleSize = images[imgObj].size;
	ri->end.scaleSize = images[imgObj].size;
	ri->start.color = CLR_WHITE;
	ri->end.color = CLR_WHITE;
	ri->start.rotation = 0.0f;
	ri->end.rotation = 0.0f;
	ri->offset = images[imgObj].offset;
	ri->flags = images[imgObj].flags;
	ri->camFlags = camFlags;
	ri->shaderType = images[imgObj].shaderType;
	ri->depth = depth;
	ri->scissorID = scissor_GetTopID( );
	memcpy( ri->uvs, DEFAULT_DRAW_INSTRUCTION.uvs, sizeof( ri->uvs ) );

	ri->uvs[0] = images[imgObj].uvMin;

	ri->uvs[1].x = images[imgObj].uvMin.x;
	ri->uvs[1].y = images[imgObj].uvMax.y;

	ri->uvs[2].x = images[imgObj].uvMax.x;
	ri->uvs[2].y = images[imgObj].uvMin.y;

	ri->uvs[3] = images[imgObj].uvMax;

	return ri;
}

/*
Adds to the list of images to draw.
*/
#define DRAW_INSTRUCTION_START \
	DrawInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth ); \
	if( ri == NULL ) { return -1; }

#define DRAW_INSTRUCTION_END \
	return 0;

#define SET_DRAW_INSTRUCTION_SCALE( startX, startY, endX, endY ) \
	ri->start.scaleSize.x *= startX; \
	ri->start.scaleSize.y *= startY; \
	ri->end.scaleSize.x *= endX; \
	ri->end.scaleSize.y *= endY;

#define SET_DRAW_INSTRUCTION_COLOR( startColor, endColor ) \
	ri->start.color = startColor; \
	ri->end.color = endColor; \
	if( ( ( startColor.a > 0 ) && ( startColor.a < 1.0f ) ) || \
		( ( endColor.a > 0 ) && ( endColor.a < 1.0f ) )) { \
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY; }

#define SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad ) \
	ri->start.rotation = startRotRad; \
	ri->end.rotation = endRotRad;

int img_Draw( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	DRAW_INSTRUCTION_END;
}

int img_Draw_s( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	DRAW_INSTRUCTION_END;
}

int img_Draw_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_c( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor,
	float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_c_r( int imgID, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, float startRotRad, float endRotRad, int8_t depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRotRad, endRotRad );
	DRAW_INSTRUCTION_END;
}

int img_Draw3x3( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth )
{
	return img_Draw3x3_c( imgUL, imgUC, imgUR, imgML, imgMC, imgMR, imgDL, imgDC, imgDR,
		camFlags, startPos, endPos, startSize, endSize, CLR_WHITE, CLR_WHITE, depth );
}

int img_Draw3x3v( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize, int8_t depth )
{
	return img_Draw3x3( imgs[0], imgs[1], imgs[2], imgs[3], imgs[4], imgs[5], imgs[6], imgs[7], imgs[8],
		camFlags, startPos, endPos, startSize, endSize, depth );
}

int img_Draw3x3_c( int imgUL, int imgUC, int imgUR, int imgML, int imgMC, int imgMR, int imgDL, int imgDC, int imgDR,
	uint32_t camFlags, Vector2 startPos, Vector2 endPos, Vector2 startSize, Vector2 endSize,
	Color startColor, Color endColor, int8_t depth )
{
	// assuming that the dimensions of each column and row are the same
	// also assumes the width and height to draw greater than the sum of the widths and heights, if it isn't we just
	//  clamp it so it's a minimum of that much

	Vector2 sliceSize;
	img_GetSize( imgUL, &sliceSize );

	float imgLeftColumnWidth = sliceSize.x;
	float imgTopRowHeight = sliceSize.y;

	img_GetSize( imgMC, &sliceSize );

	float imgMidColumnWidth = sliceSize.x;
	float imgMidRowHeight = sliceSize.y;

	img_GetSize( imgDR, &sliceSize );

	float imgRightColumnWidth = sliceSize.x;
	float imgBottomRowHeight = sliceSize.y;

	// calculate the scales
	float midWidthStart = MAX( 0.0f, ( startSize.x - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthEnd = MAX( 0.0f, ( endSize.x - imgLeftColumnWidth - imgRightColumnWidth ) );
	float midWidthScaleStart = midWidthStart / imgMidColumnWidth;
	float midWidthScaleEnd = midWidthEnd / imgMidColumnWidth;

	float midHeightStart = MAX( 0.0f, ( startSize.y - imgTopRowHeight - imgBottomRowHeight ) );
	float midHeightEnd = MAX( 0.0f, ( endSize.y - imgTopRowHeight - imgBottomRowHeight ) );
	float midHeightScaleStart = midHeightStart / imgMidRowHeight;
	float midHeightScaleEnd = midHeightEnd / imgMidRowHeight;

	// calculate the positions
	float leftPosStart = startPos.x - ( midWidthStart / 2.0f ) - ( imgLeftColumnWidth / 2.0f );
	float leftPosEnd = endPos.x - ( midWidthEnd / 2.0f ) - ( imgLeftColumnWidth / 2.0f );
	float midHorizPosStart = startPos.x;
	float midHorizPosEnd = endPos.x;
	float rightPosStart = startPos.x + ( midWidthStart / 2.0f ) + ( imgRightColumnWidth / 2.0f );
	float rightPosEnd = endPos.x + ( midWidthEnd / 2.0f ) + ( imgRightColumnWidth / 2.0f );

	float topPosStart = startPos.y - ( midHeightStart / 2.0f ) - ( imgTopRowHeight / 2.0f );
	float topPosEnd = endPos.y - ( midHeightEnd / 2.0f ) - ( imgTopRowHeight / 2.0f );
	float midVertPosStart = startPos.y;
	float midVertPosEnd = endPos.y;
	float bottomPosStart = startPos.y + ( midHeightStart / 2.0f ) + ( imgBottomRowHeight / 2.0f );
	float bottomPosEnd = endPos.y + ( midHeightEnd / 2.0f ) + ( imgBottomRowHeight / 2.0f );

	Vector2 posStart;
	Vector2 posEnd;
	Vector2 scaleStart;
	Vector2 scaleEnd;

#define DRAW_SECTION( img, startX, startY, endX, endY, startW, startH, endW, endH ) \
	posStart.x = ( startX ); posStart.y = ( startY ); \
	posEnd.x = ( endX ); posEnd.y = ( endY ); \
	scaleStart.w = ( startW ); scaleStart.h = ( startH ); \
	scaleEnd.w = ( endW ); scaleEnd.h = ( endH ); \
	if( img_Draw_sv_c( img, camFlags, posStart, posEnd, scaleStart, scaleEnd, startColor, endColor, depth ) < 0 ) return -1;

	// upper-left
	DRAW_SECTION( imgUL, leftPosStart, topPosStart, leftPosEnd, topPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// upper-center
	DRAW_SECTION( imgUC, midHorizPosStart, topPosStart, midHorizPosEnd, topPosEnd, midWidthScaleStart, 1.0f, midWidthScaleStart, 1.0f );

	// upper-right
	DRAW_SECTION( imgUR, rightPosStart, topPosStart, rightPosEnd, topPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// mid-left
	DRAW_SECTION( imgML, leftPosStart, midVertPosStart, leftPosEnd, midVertPosEnd, 1.0f, midHeightScaleStart, 1.0f, midHeightScaleEnd );

	// mid-center
	DRAW_SECTION( imgML, midHorizPosStart, midVertPosStart, midHorizPosEnd, midVertPosEnd,
		midWidthScaleStart, midHeightScaleStart, midWidthScaleEnd, midHeightScaleEnd );

	// mid-right
	DRAW_SECTION( imgMR, rightPosStart, midVertPosStart, rightPosEnd, midVertPosEnd, 1.0f, midHeightScaleStart, 1.0f, midHeightScaleEnd );

	// low-left
	DRAW_SECTION( imgDL, leftPosStart, bottomPosStart, leftPosEnd, bottomPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

	// low-center
	DRAW_SECTION( imgDC, midHorizPosStart, bottomPosStart, midHorizPosEnd, bottomPosEnd, midWidthScaleStart, 1.0f, midWidthScaleStart, 1.0f );

	// low-right
	DRAW_SECTION( imgDR, rightPosStart, bottomPosStart, rightPosEnd, bottomPosEnd, 1.0f, 1.0f, 1.0f, 1.0f );

#undef DRAW_SECTION
	return 0;
}

int img_Draw3x3v_c( int* imgs, uint32_t camFlags, Vector2 startPos, Vector2 endPos,
	Vector2 startSize, Vector2 endSize, Color startColor, Color endColor, int8_t depth )
{
	return img_Draw3x3_c( imgs[0], imgs[1], imgs[2], imgs[3], imgs[4], imgs[5], imgs[6], imgs[7], imgs[8],
		camFlags, startPos, endPos, startSize, endSize, startColor, endColor, depth );
}

#undef DRAW_INSTRUCTION_QUEUE_START
#undef DRAW_INSTRUCTION_QUEUE_END
#undef SET_DRAW_INSTRUCTION_SCALE
#undef SET_DRAW_INSTRUCTION_COLOR
#undef SET_DRAW_INSTRUCTION_ROT

/*
Clears the image draw list.
*/
void img_ClearDrawInstructions( void )
{
	lastDrawInstruction = -1;
}

static void createRenderTransform( Vector2* pos, Vector2* scale, float rot,  Vector2* offset, Matrix4* out )
{
	memcpy( out, &IDENTITY_MATRIX, sizeof( Matrix4 ) );
	// this is the fully multiplied out multiplication of the transform, scale, and z-rotation
	// pos * rot * offset * scale
	//  problem here if the scale is different in each dimension (e.g scale = { 16, 24 }), the rotation isn't taken
	//  into account so you end up with an oddly stretched image

	
	float cosRot = cosf( rot );
	float sinRot = sinf( rot );

	out->m[0] = cosRot * scale->v[0];
	out->m[1] = sinRot * scale->v[0];
	out->m[4] = -sinRot * scale->v[1];
	out->m[5] = cosRot * scale->v[1];
	//out->m[10] = scale->v[2];
	out->m[12] = pos->v[0] - ( offset->v[1] * sinRot ) + ( offset->v[0] * cosRot );
	out->m[13] = pos->v[1] + ( offset->v[0] * sinRot ) + ( offset->v[1] * cosRot );
	//out->m[14] = -pos->v[2]; //*/

	// pos * rot * scale * offset
	/*out->m[0] = cosRot * scale->v[0];
	out->m[1] = sinRot * scale->v[0];
	out->m[4] = -sinRot * scale->v[1];
	out->m[5] = cosRot * scale->v[1];

	out->m[12] = pos->v[0] + ( offset->v[0] * scale->v[0] * cosRot ) + ( offset->v[1] * scale->v[1] * -sinRot );
	out->m[13] = pos->v[1] + ( offset->v[0] * scale->v[0] * sinRot ) + ( offset->v[1] * scale->v[1] * cosRot );//*/
}

/*
Draw all the images.
*/
void img_Render( float normTimeElapsed )
{
	Vector2 unitSqVertPos[] = { { -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f } };
	GLuint indices[] = {
		0, 1, 2,
		1, 2, 3,
	};

	for( int idx = 0; idx <= lastDrawInstruction; ++idx ) {
		Vector2 verts[4];

		// generate the sprites matrix
		Matrix4 modelTf;
		Vector2 pos, sclSz;
		float rot;
		Color col;

		vec2_Lerp( &( renderBuffer[idx].start.pos ), &( renderBuffer[idx].end.pos ), normTimeElapsed, &pos );
		clr_Lerp( &( renderBuffer[idx].start.color ), &( renderBuffer[idx].end.color ), normTimeElapsed, &col );
		vec2_Lerp( &( renderBuffer[idx].start.scaleSize ), &( renderBuffer[idx].end.scaleSize ), normTimeElapsed, &sclSz );

		rot = radianRotLerp( renderBuffer[idx].start.rotation, renderBuffer[idx].end.rotation, normTimeElapsed );
		createRenderTransform( &pos, &sclSz, rot, &renderBuffer[idx].offset, &modelTf );

		for( int i = 0; i < 4; ++i ) {
			mat4_TransformVec2Pos( &modelTf, &( unitSqVertPos[i] ), &( verts[i] ) );
		}

		int transparent = ( renderBuffer[idx].flags & IMGFLAG_HAS_TRANSPARENCY ) != 0;

		triRenderer_Add( verts[indices[0]], verts[indices[1]], verts[indices[2]],
			renderBuffer[idx].uvs[indices[0]], renderBuffer[idx].uvs[indices[1]], renderBuffer[idx].uvs[indices[2]],
			renderBuffer[idx].shaderType, renderBuffer[idx].textureObj, col,
			renderBuffer[idx].scissorID, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			transparent );
		triRenderer_Add( verts[indices[3]], verts[indices[4]], verts[indices[5]],
			renderBuffer[idx].uvs[indices[3]], renderBuffer[idx].uvs[indices[4]], renderBuffer[idx].uvs[indices[5]],
			renderBuffer[idx].shaderType, renderBuffer[idx].textureObj, col,
			renderBuffer[idx].scissorID, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			transparent );
	}
}