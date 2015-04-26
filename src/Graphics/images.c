#include "images.h"

#include "../Others/glew.h"
#include <SDL_opengl.h>
#include <assert.h>
#include <stb_image.h>
#include <SDL_ttf.h>
#include <SDL_log.h>
#include <math.h>
#include <stdlib.h>

#include "../Math/matrix4.h"
#include "shaderManager.h"
#include "camera.h"
#include "triRendering.h"

/* Image loading types and variables */
#define MAX_IMAGES 512

enum {
	IMGFLAG_IN_USE = 0x1,
	IMGFLAG_HAS_TRANSPARENCY = 0x2,
};

typedef struct {
	GLuint textureObj;
	Vector2 size;
	Vector2 offset;
	int flags;
	int packageID;
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
	DrawInstructionState start;
	DrawInstructionState end;
	int flags;
	int camFlags;
	char depth;
} DrawInstruction;

static DrawInstruction renderBuffer[MAX_RENDER_INSTRUCTIONS];
static int lastDrawInstruction;

static const DrawInstruction DEFAULT_DRAW_INSTRUCTION = {
	0, -1, { 0.0f, 0.0f },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f },
	{ { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f },
	0, 0, 0
};

/*
Initializes images.
 Returns < 0 on an error.
*/
int img_Init( void )
{
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
Binds a surface to an OpenGL texture. Returns 0 if it's successful, -1 on failure.
*/
static int bindSDLSurface( SDL_Surface* surface, GLuint* texObjID )
{
	// convert the pixels into a texture
	GLenum texFormat;

	if( surface->format->BytesPerPixel == 4 ) {
		texFormat = GL_RGBA;
	} else if( surface->format->BytesPerPixel == 3 ) {
		texFormat = GL_RGB;
	} else {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to handle format!" );
		return -1;
	}

	glGenTextures( 1, texObjID );

	glBindTexture( GL_TEXTURE_2D, *texObjID );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTexImage2D( GL_TEXTURE_2D, 0, texFormat, surface->w, surface->h, 0, texFormat, GL_UNSIGNED_BYTE, surface->pixels );

	return 0;
}

/*
Tests for any alpha values on the SDL_Surface that are > 0 and < 255.
 Returns if there is any transparency in the object.
*/
int testSurfaceTransparency( SDL_Surface* surface )
{
	Uint8 r, g, b, a;
	int bpp = surface->format->BytesPerPixel;

	for( int y = 0; y < surface->h; ++y ) {
		for( int x = 0; x < surface->w; ++x ) {
			Uint32 pixel;
			// pitch seems to be in bits, not bytes as the documentation says it should be
			Uint8 *pos = ( ( (Uint8*)surface->pixels ) + ( ( y * ( surface->pitch / 8 ) ) + ( x * bpp ) ) );
			switch( bpp ) {
			case 3:
				if( SDL_BYTEORDER == SDL_BIG_ENDIAN ) {
					pixel = ( pos[0] << 16 ) | ( pos[1] << 8 ) | pos[2];
				} else {
					pixel = pos[0] | ( pos[1] << 8 ) | ( pos[2] << 16 );
				}
				break;
			case 4:
				pixel = *( (Uint32*)pos );
				break;
			default:
				pixel = 0;
				break;
			}
			SDL_GetRGBA( pixel, surface->format, &r, &g, &b, &a );
			if( ( a > 0x00 ) && ( a < 0xFF ) ) {
				return 1;
			}
		}
	}

	return 0;
}

/*
Loads the image stored at file name.
 Returns the index of the image on success.
 Returns -1 on failure, and prints a message to the log.
*/
int img_Load( const char* fileName )
{
	int width, height, comp;
	SDL_Surface* loadSurface = NULL;
	unsigned char* imageData = NULL;
	int newIdx = -1;

	// find the first empty spot, make sure we won't go over our maximum
	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to load image %s! Image storage full.", fileName );
		goto clean_up;
	}

	// load and convert file to pixel data
	imageData = stbi_load( fileName, &width, &height, &comp, 4 );
	if( imageData == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to load image %s! STB Error: %s", fileName, stbi_failure_reason( ) );
		newIdx = -1;
		goto clean_up;
	}
	
	int bitsPerPixel = comp * 8;
	loadSurface = SDL_CreateRGBSurfaceFrom( (void*)imageData, width, height, bitsPerPixel, ( width * bitsPerPixel ), 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 );
	if( loadSurface == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to create RGB surface for %s! SDL Error: %s", fileName, SDL_GetError( ) );
		newIdx = -1;
		goto clean_up;
	}

	if( bindSDLSurface( loadSurface, &( images[newIdx].textureObj ) ) < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to convert surface to texture for %s! SDL Error: %s", fileName, SDL_GetError( ) );
		newIdx = -1;
		goto clean_up;
	} 

	images[newIdx].size.v[0] = (float)width;
	images[newIdx].size.v[1] = (float)height;
	images[newIdx].offset = VEC2_ZERO;
	images[newIdx].packageID = -1;
	images[newIdx].flags = IMGFLAG_IN_USE;
	//if( testSurfaceTransparency( loadSurface ) ) {
	if( testSurfaceTransparency( loadSurface ) ) {
		images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

clean_up:
	SDL_FreeSurface( loadSurface );
	stbi_image_free( imageData );
	return newIdx;
}

/*
Creates an image from a surface.
*/
int img_Create( SDL_Surface* surface )
{
	int newIdx;

	assert( surface != NULL );

	newIdx = findAvailableImageIndex( );
	if( newIdx < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to create image from surface! Image storage full." );
		return -1;
	}

	if( bindSDLSurface( surface, &( images[newIdx].textureObj ) ) < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to convert surface to texture! SDL Error: %s", SDL_GetError( ) );
		return -1;
	} else {
		images[newIdx].size.v[0] = (float)surface->w;
		images[newIdx].size.v[1] = (float)surface->h;
		images[newIdx].offset = VEC2_ZERO;
		images[newIdx].packageID = -1;
		images[newIdx].flags = IMGFLAG_IN_USE;
		if( testSurfaceTransparency( surface ) ) {
			images[newIdx].flags |= IMGFLAG_HAS_TRANSPARENCY;
		}
	}

	return newIdx;
}

/*
Creates an image from a string, used for rendering text.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int img_CreateText( const char* text, const char* fontName, int pointSize, Color color, Uint32 wrapLen )
{
	return img_UpdateText( text, fontName, pointSize, color, wrapLen, -1 );
}

/*
Changes the existing image instead of trying to create a new one.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int img_UpdateText( const char* text, const char* fontName, int pointSize, Color color, Uint32 wrapLen, int existingImage )
{
	assert( existingImage < MAX_IMAGES );
	assert( images[existingImage].flags & IMGFLAG_IN_USE );
	assert( text != NULL );
	assert( fontName != NULL );

	int newIdx;
	TTF_Font* font;
	SDL_Surface* textSurface;

	/* doesn't already exist, find the first empty spot, make sure we won't go over our maximum */
	if( existingImage < 0 ) {
		newIdx = findAvailableImageIndex( );
		if( newIdx < 0 ) {
			SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to create text image! Image storage full." );
			return -1;
		}
	} else {
		newIdx = existingImage;
	}

	/* screw performance, just load the font every single time, can change this later */
	font = TTF_OpenFont( fontName, pointSize );
	if( font == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, TTF_GetError( ) );
		return -1;
	}

	if( wrapLen == 0 ) {
		textSurface = TTF_RenderText_Blended( font, text, clr_ToSDLColor( &color ) );
	} else {
		textSurface = TTF_RenderText_Blended_Wrapped( font, text, clr_ToSDLColor( &color ), wrapLen );
	}

	if( textSurface == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to render text! SDL Error: %s", TTF_GetError( ) );
		newIdx = -1;
	} else {
		if( bindSDLSurface( textSurface, &( images[newIdx].textureObj ) ) < 0 ) {
			SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Unable to convert surface to texture for text! SDL Error: %s", SDL_GetError( ) );
			newIdx = -1;
		} else {
			images[newIdx].size.v[0] = (float)textSurface->w;
			images[newIdx].size.v[1] = (float)textSurface->h;
			images[newIdx].offset = VEC2_ZERO;
			images[newIdx].flags = IMGFLAG_IN_USE;
			images[newIdx].packageID = -1;
		}
		SDL_FreeSurface( textSurface );
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

	glDeleteTextures( 1, &( images[idx].textureObj ) );
	images[idx].size.v[0] = 0.0f;
	images[idx].size.v[1] = 0.0f;
	images[idx].flags = 0;
	images[idx].packageID = -1;
}

/*
Takes in a list of file names and loads them together. It's assumed the length of fileNames and retIDs equals count.
 Returns the package ID used to clean up later, returns -1 if there's a problem.
*/
int img_LoadPackage( int count, char** fileNames, int* retIDs )
{
	// TODO: switch this over to loading everything into an atlas
	int currPackageID = 0;
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID <= currPackageID ) ) {
			currPackageID = images[i].packageID + 1;
		}
	}

	if( currPackageID < 0 ) {
		return -1;
	}

	for( int i = 0; i < count; ++i ) {
		retIDs[i] = img_Load( fileNames[i] );
		if( retIDs[i] >= 0 ) {
			images[retIDs[i]].packageID = currPackageID;
		} else {
			SDL_LogError( SDL_LOG_CATEGORY_RENDER, "Problem loading image %s into a package.", fileNames[i] );
		}
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
	assert( images[idx].flags & IMGFLAG_IN_USE );

	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	images[idx].offset = offset;
}

/*
initializes the structure so only what's used needs to be set, also fill in
  some stuff that all of the queueRenderImage functions use, returns the pointer
  for further setting of other stuff
 returns NULL if there's a problem
*/
static DrawInstruction* GetNextRenderInstruction( int imgObj, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth )
{
	if( !( images[imgObj].flags & IMGFLAG_IN_USE ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Attempting to draw invalid image: %i", imgObj );
		return NULL;
	}

	if( lastDrawInstruction >= MAX_RENDER_INSTRUCTIONS ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Render instruction queue full." );
		return NULL;
	}

	++lastDrawInstruction;
	DrawInstruction* ri = &( renderBuffer[lastDrawInstruction] );

	*ri = DEFAULT_DRAW_INSTRUCTION;
	ri->textureObj = images[imgObj].textureObj;
	ri->imageObj = imgObj;
	ri->start.pos = startPos;
	ri->end.pos = endPos;
	//vec2ToVec3( &startPos, (float)depth + ( Z_ORDER_OFFSET * lastDrawInstruction ), &( ri->start.pos ) );
	//vec2ToVec3( &endPos, (float)depth + ( Z_ORDER_OFFSET * lastDrawInstruction ), &( ri->end.pos ) );
	ri->start.scaleSize = images[imgObj].size;
	ri->end.scaleSize = images[imgObj].size;
	ri->start.color = CLR_WHITE;
	ri->end.color = CLR_WHITE;
	ri->start.rotation = 0.0f;
	ri->end.rotation = 0.0f;
	ri->offset = images[imgObj].offset;
	ri->flags = images[imgObj].flags;
	ri->camFlags = camFlags;

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

#define SET_DRAW_INSTRUCTION_ROT( startRot, endRot ) \
	ri->start.rotation = startRot; \
	ri->end.rotation = endRot;

int img_Draw( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth )
{
	DRAW_INSTRUCTION_START;
	DRAW_INSTRUCTION_END;
}

int img_Draw_s( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	DRAW_INSTRUCTION_END;
}

int img_Draw_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
}

int img_Draw_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor,
	float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
}

int img_Draw_s_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale, startScale, endScale, endScale );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
}

int img_Draw_sv_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	Color startColor, Color endColor, float startRot, float endRot, char depth )
{
	DRAW_INSTRUCTION_START;
	SET_DRAW_INSTRUCTION_SCALE( startScale.x, startScale.y, endScale.x, endScale.y );
	SET_DRAW_INSTRUCTION_COLOR( startColor, endColor );
	SET_DRAW_INSTRUCTION_ROT( startRot, endRot );
	DRAW_INSTRUCTION_END;
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
	//out->m[14] = -pos->v[2];
}

/*
Draw all the images.
*/
void img_Render( float normTimeElapsed )
{
	Vector2 unitSqVertPos[] = { { -0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f } };
	Vector2 unitSqVertUV[] = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
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
			unitSqVertUV[indices[0]], unitSqVertUV[indices[1]], unitSqVertUV[indices[2]],
			renderBuffer[idx].textureObj, col, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			transparent );
		triRenderer_Add( verts[indices[3]], verts[indices[4]], verts[indices[5]],
			unitSqVertUV[indices[3]], unitSqVertUV[indices[4]], unitSqVertUV[indices[5]],
			renderBuffer[idx].textureObj, col, renderBuffer[idx].camFlags, renderBuffer[idx].depth,
			transparent );
	}
}