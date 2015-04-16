#include "graphics.h"

/*
TODO: Split off into different cameras. Each camera will be set when it's rendering.
*/

//#include <SDL_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL_ttf.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../Others/glew.h"
#include <SDL_opengl.h>

#include "camera.h"
#include "../Math/MathUtil.h"
#include "ShaderManager.h"

/* Image loading types and variables */
#define MAX_IMAGES 512

// bound texture with size information, if both dimensions of size is 0 then it's not in use
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
	Vector3 pos;
	Vector2 scaleSize;
	Color color;
	float rotation;
} RenderInstructionState;

typedef struct {
	GLuint textureObj;
	int imageObj;
	Vector2 offset;
	RenderInstructionState start;
	RenderInstructionState end;
	int flags;
	int camFlags;
} RenderInstruction;

static RenderInstruction renderBuffer[MAX_RENDER_INSTRUCTIONS];
static int lastRenderInstruction;

/*
PLANNING!
How to do buffers involved with just geometry work?
Will probably make sprites a special case of general geometry
typedef struct {
	RenderInstruction buffer[MAX_RENDER_INSTRUCTIONS];
	int lastInstruction;
	sortFunction;
	renderFunction;
} RenderBuffer;

// note: the render buffers are processed in this order
typedef enum {
	RT_SOLID,
	RT_TRANSPARENT,
	NUM_RENDER_TYPES
} RenderTypes;

static RenderBuffer renderBuffers[NUM_RENDER_TYPES];

maybe have three separate renderers that are all processed by the graphics
so you'd have the sprite, debug, and geometry renderers that all expected
different data, the only problem with that is any transparency in the sprite
and geometry data
*/

static const RenderInstruction DEFAULT_RENDER_INSTRUCTION = {
	0, -1, { 0.0f, 0.0f },
	{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0xFF, 0xFF, 0xFF, 0xFF }, 0.0f },
	{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0xFF, 0xFF, 0xFF, 0xFF }, 0.0f }
};

// debug rendering will always be done with lines, don't need it to be too fast as we should only be using this when we absolutely need it
//  MAX_DEBUG_VERTS must be even
#define MAX_DEBUG_VERTS ( 1024 * 2 )
#if MAX_DEBUG_VERTS % 2 == 1
	#error "MAX_DEBUG_VERTS needs to be divisible by 2."
#endif

typedef struct {
	Vector3 pos;
	Color color;
	unsigned int camFlags;
} DebugVertex;

static DebugVertex debugBuffer[MAX_DEBUG_VERTS];
static int lastDebugVert;

static GLuint debugIndicesBuffer[MAX_DEBUG_VERTS];
static int lastDebugIndex;

GLuint defaultVAO;

GLuint debugVAO;
GLuint debugVBO;
GLuint debugIBO;

static SDL_GLContext glContext;

static float currentTime;
static float endTime;

static float clearRed;
static float clearGreen;
static float clearBlue;
static float clearAlpha;

/* Used for sorting, how close we want to count as the same, should always be >= MAX_RENDER_INSTRUCTIONS to avoid glitches */
#define MIN_SORT_RESOLUTION 2000.0f
#define Z_ORDER_OFFSET ( 1.0f / (float)( MAX_RENDER_INSTRUCTIONS + 1 ) )

static Color white = { 1.0f, 1.0f, 1.0f, 1.0f };

typedef struct {
	GLuint VAO;
	GLuint VBO;
	GLuint IBO;
} GeomData;

static GeomData unitSquare;

enum ShaderProgramTypes {
	SP_SPRITE,
	SP_DEBUG,
	NUM_SHADER_PROGRAMS
};
static ShaderProgram shaderPrograms[NUM_SHADER_PROGRAMS];

/* some error check dumping, returns if there was an error */
int checkAndLogError( const char* extraInfo )
{
	GLenum error = glGetError( );

	if( error == GL_NO_ERROR ) {
		return 0;
	}

#ifdef _DEBUG
	char* errorMsg;
	switch( error ) {
	case GL_INVALID_ENUM:
		errorMsg = "Invalid enumeration";
		break;
	case GL_INVALID_VALUE:
		errorMsg = "Invalid value";
		break;
	case GL_INVALID_OPERATION:
		errorMsg = "Invalid operation";
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		errorMsg = "Invalid framebuffer operation";
		break;
	case GL_OUT_OF_MEMORY:
		errorMsg = "Out of memory";
		break;
	default:
		errorMsg = "Unknown error";
		break;
	}

	if( extraInfo == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Error: %s", errorMsg );
	} else {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Error: %s at %s", errorMsg, extraInfo );
	}
#endif

	return 1;
}

/*  ======= Package Loading ======= */
/*
Takes in a list of file names and loads them together. It's assumed the length of fileNames and retIDs equals count.
 Returns the package ID used to clean up later, returns -1 if there's a problem.
*/
int loadImagePackage( int count, char** fileNames, int* retIDs )
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
		retIDs[i] = loadImage( fileNames[i] );
		if( retIDs[i] >= 0 ) {
			images[retIDs[i]].packageID = currPackageID;
		}
	}

	return currPackageID;
}

/*
Cleans up all the images in an image package. The package ID passed in will be invalid after this.
*/
void cleanPackage( int packageID )
{
	for( int i = 0; i < MAX_IMAGES; ++i ) {
		if( ( images[i].flags & IMGFLAG_IN_USE ) && ( images[i].packageID == packageID ) ) {
			cleanImageAt( i );
		}
	}
}

/*
Initializes the stored images as well as the image loading.
 Returns >=0 on success.
*/
int initImages( void )
{
	memset( images, 0, sizeof(images) );

	return 0;
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
	checkAndLogError( "Binding surface" );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTexImage2D( GL_TEXTURE_2D, 0, texFormat, surface->w, surface->h, 0, texFormat, GL_UNSIGNED_BYTE, surface->pixels );

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
int loadImage( const char* fileName )
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
int createImage( SDL_Surface* surface )
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
Creates an image from a string, used for rendering text. Can be passed in
 an existing image to overwrite it's spot, send a negative number otherwise.
 Returns the index of the image on success.
 Reutrns -1 on failure, prints a message to the log, and doesn't overwrite if a valid existing image id is passed in.
*/
int createNewTextImage( const char* text, const char* fontName, int pointSize, SDL_Color color, Uint32 wrapLen )
{
	return createTextImage( text, fontName, pointSize, color, wrapLen, -1 );
}

int createTextImage( const char* text, const char* fontName, int pointSize, SDL_Color color, Uint32 wrapLen, int existingImage )
{
	int newIdx;
	TTF_Font* font;
	SDL_Surface* textSurface;

	assert( text != NULL );
	assert( fontName != NULL );

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
		textSurface = TTF_RenderText_Blended( font, text, color );
	} else {
		textSurface = TTF_RenderText_Blended_Wrapped( font, text, color, wrapLen );
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
void cleanImageAt( int idx )
{
	int bufIdx;

	if( ( idx < 0 ) || ( ( images[idx].size.v[0] == 0.0f ) && ( images[idx].size.v[1] == 0.0f ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	/* clean up anything we're wanting to draw */
	for( bufIdx = 0; bufIdx <= lastRenderInstruction; ++bufIdx ) {
		if( renderBuffer[bufIdx].imageObj == idx ) {
			if( lastRenderInstruction > 0 ) {
				renderBuffer[bufIdx] = renderBuffer[lastRenderInstruction-1];
			}
			--lastRenderInstruction;
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
Sets an offset to render the image from.
*/
void setImageOffset( int idx, Vector2 offset )
{
	if( ( idx < 0 ) || ( !( images[idx].flags & IMGFLAG_IN_USE ) ) || ( idx >= MAX_IMAGES ) ) {
		return;
	}

	images[idx].offset = offset;
}

/*
Loads all the default shaders we need. All text is defined in the program, may move it out into it's own shader files later.
 Returns 1 on failure, 0 on success.
*/
static int loadDefaultShaders( void )
{
	//  the base shaders
	ShaderDefinition defaultShaderDefs[4];
	ShaderProgramDefinition defaultProgDefs[NUM_SHADER_PROGRAMS];

	// Sprite shader
	defaultShaderDefs[0].fileName = NULL;
	defaultShaderDefs[0].type = GL_VERTEX_SHADER;
	defaultShaderDefs[0].shaderText =	"#version 330\n"
										"uniform mat4 mvpMatrix;\n"
										"layout(location = 0) in vec4 vVertex;\n"
										"layout(location = 1) in vec2 vTexCoord0;\n"
										"out vec2 vTex;\n"
										"out vec4 vCol;\n"
										"void main( void )\n"
										"{\n"
										"	vTex = vTexCoord0;\n"
										"	gl_Position = mvpMatrix * vVertex;\n"
										"}\n";

	defaultShaderDefs[1].fileName = NULL;
	defaultShaderDefs[1].type = GL_FRAGMENT_SHADER;
	defaultShaderDefs[1].shaderText =	"#version 330\n"
										"in vec2 vTex;\n"
										"uniform sampler2D textureUnit0;\n"
										"uniform vec4 vCol;\n"
										"out vec4 outCol;\n"
										"void main( void )\n"
										"{\n"
										"	outCol = texture2D(textureUnit0, vTex) * vCol;\n"
										"	if( outCol.w <= 0.0f ) {\n"
										"		discard;\n"
										"	}\n"
										"}\n";
	
	// debug shader
	defaultShaderDefs[2].fileName = NULL;
	defaultShaderDefs[2].type = GL_VERTEX_SHADER;
	defaultShaderDefs[2].shaderText =	"#version 330\n"
										"uniform mat4 mvpMatrix;\n"
										"layout(location = 0) in vec4 vertex;\n"
										"layout(location = 2) in vec4 color;\n"
										"out vec4 vertCol;\n"
										"void main( void )\n"
										"{\n"
										"	vertCol = color;\n"
										"	gl_Position = mvpMatrix * vertex;\n"
										"}\n";

	defaultShaderDefs[3].fileName = NULL;
	defaultShaderDefs[3].type = GL_FRAGMENT_SHADER;
	defaultShaderDefs[3].shaderText =	"#version 330\n"
										"in vec4 vertCol;\n"
										"out vec4 outCol;\n"
										"void main( void )\n"
										"{\n"
										"	outCol = vertCol;\n"
										"}\n";

	defaultProgDefs[SP_SPRITE].fragmentShader = 1;
	defaultProgDefs[SP_SPRITE].vertexShader = 0;
	defaultProgDefs[SP_SPRITE].geometryShader = -1;
	defaultProgDefs[SP_SPRITE].uniformNames = "mvpMatrix textureUnit0 vCol";

	defaultProgDefs[SP_DEBUG].fragmentShader = 3;
	defaultProgDefs[SP_DEBUG].vertexShader = 2;
	defaultProgDefs[SP_DEBUG].geometryShader = -1;
	defaultProgDefs[SP_DEBUG].uniformNames = "mvpMatrix";

	if( loadShaders( &( defaultShaderDefs[0] ), sizeof( defaultShaderDefs ) / sizeof( ShaderDefinition ),
		&( defaultProgDefs[0] ), &( shaderPrograms[0] ), NUM_SHADER_PROGRAMS ) <= 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Error compiling default shaders.\n" );
		return -1;
	}

	return 0;
}

/*
Creates the geometry used for the sprite rendering.
 Returns -1 on failure, 0 on success.
*/
static int createDefaultGeometry( void )
{
	//  the unit square used for sprites
	Vector3 unitSqVertPos[] = { { -0.5f, -0.5f, 0.0f }, { -0.5f, 0.5f, 0.0f }, { 0.5f, -0.5f, 0.0f }, { 0.5f, 0.5f, 0.0f } };
	Vector2 unitSqVertUV[] = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
	GLuint spriteIndexData[] = {
		0, 1, 2,
		1, 2, 3,
	};

	glGenVertexArrays( 1, &defaultVAO );
	if( defaultVAO == 0 ) {
		return -1;
	}

	// first the sprite stuff *****
	//  generate the stuff
	glGenVertexArrays( 1, &( unitSquare.VAO ) );
	glGenBuffers( 1, &( unitSquare.VBO ) );
	glGenBuffers( 1, &( unitSquare.IBO ) );
	if( ( unitSquare.VAO == 0 ) || ( unitSquare.VBO == 0 ) || ( unitSquare.IBO == 0 ) ) {
		return -1;
	}

	//  set all the stuff
	glBindVertexArray( unitSquare.VAO );
	glBindBuffer( GL_ARRAY_BUFFER, unitSquare.VBO );

	glBufferData( GL_ARRAY_BUFFER, sizeof( unitSqVertPos ) + sizeof( unitSqVertUV ), NULL, GL_STATIC_DRAW );
	glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( unitSqVertPos ), &( unitSqVertPos[0].v[0] ) );
	glBufferSubData( GL_ARRAY_BUFFER, sizeof( unitSqVertPos ), sizeof( unitSqVertUV ), &( unitSqVertUV[0].v[0] ) );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, unitSquare.IBO );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( spriteIndexData ), spriteIndexData, GL_STATIC_DRAW );

	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0 );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)( sizeof( unitSqVertPos ) ) );

	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 1 );

	glBindVertexArray( 0 );

	// next create the buffers for the debug rendering *****
	glGenBuffers( 1, &debugVBO );
	if( debugVBO == 0 ) {
		return -1;
	}
	glGenBuffers( 1, &debugIBO );
	if( debugIBO == 0 ) {
		return -1;
	}

	glGenVertexArrays( 1, &debugVAO );
	if( debugVAO == 0 ) {
		return -1;
	}

	// reserve buffers for the debug data
	glBindVertexArray( debugVAO );

	glBindBuffer( GL_ARRAY_BUFFER, debugVBO );
	glBufferData( GL_ARRAY_BUFFER, sizeof( debugBuffer ), NULL, GL_DYNAMIC_DRAW );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, debugIBO );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( debugIndicesBuffer ), debugIndicesBuffer, GL_DYNAMIC_DRAW );

	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( DebugVertex ), (const GLvoid*)offsetof( DebugVertex, pos ) );
	glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( DebugVertex ), (const GLvoid*)offsetof( DebugVertex, color ) );

	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 2 );
	
	glBindVertexArray( 0 );

	return 0;
}

/*
Initial setup for the rendering instruction buffer.
 Returns 0 on success.
*/
int initRendering( SDL_Window* window )
{
	lastRenderInstruction = -1;
	lastDebugVert = -1;

	// setup opengl
	glContext = SDL_GL_CreateContext( window );
	if( glContext == NULL ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Error in initRendering while creating context: %s", SDL_GetError( ) );
		return -1;
	}
	SDL_GL_MakeCurrent( window, glContext );

	// setup glew
	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit( );
	if( glewError != GLEW_OK ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Error in initRendering while initializing GLEW: %s", (char*)glewGetErrorString( glewError ) );
		return -1;
	}
	glGetError( ); // reset error flag, glew can set it but it isn't important

	// use v-sync, avoid tearing
	if( SDL_GL_SetSwapInterval( 1 ) < 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, SDL_GetError( ) );
		return -1;
	}

	currentTime = 0.0f;
	endTime = 0.0f;

	/* init ttf stuff */
	if( TTF_Init( ) != 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, TTF_GetError( ) );
		return -1;
	}

	// create the base stuff for sprites
	if( createDefaultGeometry( ) < 0 ) {
		return -1;
	}

	if( loadDefaultShaders( ) < 0 ) {
		return -1;
	}

	return 0;
}

/*
Sets clearing color.
*/
void setRendererClearColor( float red, float green, float blue, float alpha )
{
	clearRed = red;
	clearGreen = green;
	clearBlue = blue;
	clearAlpha = alpha;
}

// initializes the structure so only what's used needs to be set, also fill in
//  some stuff that all of the queueRenderImage functions use, returns the pointer
//  for further setting of other stuff
// returns NULL if there's a problem
static RenderInstruction* GetNextRenderInstruction( int imgObj, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth )
{
	if( !( images[imgObj].flags & IMGFLAG_IN_USE ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Attempting to draw invalid image: %i", imgObj );
		return NULL;
	}

	if( lastRenderInstruction >= MAX_RENDER_INSTRUCTIONS ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Render instruction queue full." );
		return NULL;
	}

	++lastRenderInstruction;
	RenderInstruction* ri = &( renderBuffer[lastRenderInstruction] );

	*ri = DEFAULT_RENDER_INSTRUCTION;
	ri->textureObj = images[imgObj].textureObj;
	ri->imageObj = imgObj;
	vec2ToVec3( &startPos, (float)depth + ( Z_ORDER_OFFSET * lastRenderInstruction ), &( ri->start.pos ) );
	vec2ToVec3( &endPos, (float)depth + ( Z_ORDER_OFFSET * lastRenderInstruction ), &( ri->end.pos ) );
	ri->start.scaleSize = images[imgObj].size;
	ri->end.scaleSize = images[imgObj].size;
	ri->start.color = white;
	ri->end.color = white;
	ri->start.rotation = 0.0f;
	ri->end.rotation = 0.0f;
	ri->offset = images[imgObj].offset;
	ri->flags = images[imgObj].flags;
	ri->camFlags = camFlags;

	return ri;
}

/*
Adds a render instruction to the buffer.
 Returns 0 on success. Prints an error message to the log if it fails.
*/
int queueRenderImage( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}
	return 0;
}

int queueRenderImage_s( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	vec2_Scale( &( ri->start.scaleSize ), startScale, &( ri->start.scaleSize ) );
	vec2_Scale( &( ri->end.scaleSize ), endScale, &( ri->end.scaleSize ) );

	return 0;
}

int queueRenderImage_sv( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	vec2_HadamardProd( &startScale, &( ri->start.scaleSize ), &( ri->start.scaleSize ) );
	vec2_HadamardProd( &endScale, &( ri->start.scaleSize ), &( ri->end.scaleSize ) );

	return 0;
}

int queueRenderImage_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Color startColor, Color endColor, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	ri->start.color = startColor;
	ri->end.color = endColor;

	if( ( ( startColor.a > 0 ) && ( startColor.a < 0xFF ) ) ||
		( ( endColor.a > 0 ) && ( endColor.a < 0xFF ) )) {
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	return 0;
}

int queueRenderImage_s_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	Color startColor, Color endColor, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	vec2_Scale( &( ri->start.scaleSize ), startScale, &( ri->start.scaleSize ) );
	vec2_Scale( &( ri->end.scaleSize ), endScale, &( ri->end.scaleSize ) );
	ri->start.color = startColor;
	ri->end.color = endColor;

	if( ( ( startColor.a > 0 ) && ( startColor.a < 0xFF ) ) ||
		( ( endColor.a > 0 ) && ( endColor.a < 0xFF ) )) {
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	return 0;
}

int queueRenderImage_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	ri->start.rotation = startRot;
	ri->end.rotation = endRot;

	return 0;
}

int queueRenderImage_s_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startScale, float endScale,
	float startRot, float endRot, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	vec2_Scale( &( ri->start.scaleSize ), startScale, &( ri->start.scaleSize ) );
	vec2_Scale( &( ri->end.scaleSize ), endScale, &( ri->end.scaleSize ) );
	ri->start.rotation = startRot;
	ri->end.rotation = endRot;

	return 0;
}

int queueRenderImage_sv_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	vec2_HadamardProd( &startScale, &( ri->start.scaleSize ), &( ri->start.scaleSize ) );
	vec2_HadamardProd( &endScale, &( ri->start.scaleSize ), &( ri->end.scaleSize ) );
	ri->start.rotation = startRot;
	ri->end.rotation = endRot;

	return 0;
}

int queueRenderImage_r_c( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, float startRot, float endRot, Color startColor, Color endColor, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	ri->start.color = startColor;
	ri->end.color = endColor;
	ri->start.rotation = startRot;
	ri->end.rotation = endRot;

	if( ( ( startColor.a > 0 ) && ( startColor.a < 0xFF ) ) ||
		( ( endColor.a > 0 ) && ( endColor.a < 0xFF ) )) {
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	return 0;
}

int queueRenderImage_sv_c_r( int imgID, unsigned int camFlags, Vector2 startPos, Vector2 endPos, Vector2 startScale, Vector2 endScale,
	float startRot, float endRot, Color startColor, Color endColor, char depth )
{
	RenderInstruction* ri = GetNextRenderInstruction( imgID, camFlags, startPos, endPos, depth );
	if( ri == NULL ) {
		return -1;
	}

	ri->start.color = startColor;
	ri->end.color = endColor;
	ri->start.rotation = startRot;
	ri->end.rotation = endRot;
	ri->start.scaleSize = startScale;
	ri->end.scaleSize = endScale;

	if( ( ( startColor.a > 0 ) && ( startColor.a < 0xFF ) ) ||
		( ( endColor.a > 0 ) && ( endColor.a < 0xFF ) )) {
		ri->flags |= IMGFLAG_HAS_TRANSPARENCY;
	}

	return 0;
}

static int sortRenderInstructions( const void* p1, const void* p2 )
{
	// returns <0 if p1 goes before p2
	//         =0 if p1 and p2 go in the same spot
	//         >0 if p1 goes after p2
	// will sort things into two groups, those with transparency and those without
	// the without group should come first, and be further sorted by the image used, to avoid state changes
	// the with group should come last and will need to be sorted by depth
	RenderInstruction* ri1 = (RenderInstruction*)p1;
	RenderInstruction* ri2 = (RenderInstruction*)p2;

	if( ri1->flags & IMGFLAG_HAS_TRANSPARENCY ) {
		if( ri2->flags & IMGFLAG_HAS_TRANSPARENCY ) {
			// sort by z
			return ( ( ( ri2->start.pos.z ) - ( ri1->start.pos.z ) ) > 0.0f ) ? 1 : -1;
		} else {
			// p2 should go first
			return 1;
		}
	} else {
		if( ri2->flags & IMGFLAG_HAS_TRANSPARENCY ) {
			// p1 should go first
			return -1;
		} else {
			// sort by image
			return ( ( ri1->imageObj ) - ( ri2->imageObj ) );
		}
	}
}

static int queueDebugVert( unsigned int camFlags, Vector2 pos, float red, float green, float blue )
{
	if( lastDebugVert >= ( MAX_DEBUG_VERTS - 1 ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Debug instruction queue full." );
		return -1;
	}

	++lastDebugVert;
	vec2ToVec3( &pos, 0.0f, &( debugBuffer[lastDebugVert].pos ) );
	debugBuffer[lastDebugVert].color.r = red;
	debugBuffer[lastDebugVert].color.g = green;
	debugBuffer[lastDebugVert].color.b = blue;
	debugBuffer[lastDebugVert].color.a = 1.0f;
	debugBuffer[lastDebugVert].camFlags = camFlags;

	return 0;
}

int queueDebugDraw_AABB( unsigned int camFlags, Vector2 topLeft, Vector2 size, float red, float green, float blue )
{
	int fail = 0;
	Vector2 corners[4];
	corners[0] = topLeft;

	corners[1] = topLeft;
	corners[1].v[0] += size.v[0];

	vec2_Add( &topLeft, &size, &corners[2] );

	corners[3] = topLeft;
	corners[3].v[1] += size.v[1];

	for( int i = 0; i < 4; ++i ) {
		fail = fail || queueDebugDraw_Line( camFlags, corners[i], corners[(i+1)%4], red, green, blue );
	}

	return -fail;
}

int queueDebugDraw_Line( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, float red, float green, float blue )
{
	if( queueDebugVert( camFlags, pOne, red, green, blue ) ) {
		return -1;
	}
	if( queueDebugVert( camFlags, pTwo, red, green, blue ) ) {
		return -1;
	}

	return 0;
}

int queueDebugDraw_Circle( unsigned int camFlags, Vector2 center, float radius, float red, float green, float blue )
{
	int fail;
#define NUM_CIRC_VERTS 8
	Vector2 circPos[NUM_CIRC_VERTS];

	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		circPos[i].v[0] = center.v[0] + ( sinf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
		circPos[i].v[1] = center.v[1] + ( cosf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
	}

	fail = 0;
	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		fail = fail || queueDebugDraw_Line( camFlags, circPos[i], circPos[( i + 1 ) % NUM_CIRC_VERTS], red, green, blue );
	}
#undef NUM_CIRC_SEGMENTS

	return -fail;
}

void clearRenderQueue( float timeToEnd )
{
	lastRenderInstruction = -1;
	lastDebugVert = -1;
	endTime = timeToEnd;
	currentTime = 0.0f;
}

static void createRenderTransform( Vector3* pos, Vector3* scale, float rot,  Vector2* offset, Matrix4* out )
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
	out->m[10] = scale->v[2];
	out->m[12] = pos->v[0] - ( offset->v[1] * sinRot ) + ( offset->v[0] * cosRot );
	out->m[13] = pos->v[1] + ( offset->v[0] * sinRot ) + ( offset->v[1] * cosRot );
	out->m[14] = -pos->v[2];
}

/*
Goes through everything in the render buffer and does the actual rendering.
Resets the render buffer.
*/
void renderImages( float dt )
{
	int idx;
	float t;
	Matrix4 vpMat;

	currentTime += dt;
	t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );

	/* clear the screen */
	glClearColor( clearRed, clearGreen, clearBlue, clearAlpha );
	glDepthMask( GL_TRUE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glClear( GL_COLOR_BUFFER_BIT );

	glDisable( GL_CULL_FACE );
	
	/* sort by render order */
	qsort( renderBuffer, ( lastRenderInstruction + 1 ), sizeof( RenderInstruction ), sortRenderInstructions );

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glUseProgram( shaderPrograms[SP_SPRITE].programID );
	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		unsigned int camFlags = cam_GetFlags( currCamera );
		cam_GetVPMatrix( currCamera, &vpMat );

		glClear( GL_DEPTH_BUFFER_BIT );

		// draw everything
		//  we're assuming everything will be using the standard sprite shader for right now
		// TODO: Make this better, batch everything, put as many images as possible into an atlas
		// TODO: Add in multiple cameras so we can separate gameplay and ui rendering
		glBindVertexArray( unitSquare.VAO );

		GLuint lastBoundTexture = 0;
		for( idx = 0; idx <= lastRenderInstruction; ++idx ) {

			if( ( renderBuffer[idx].camFlags & camFlags ) == 0 ) {
				continue;
			}

			// generate the sprites matrix
			Matrix4 modelTf, mvpMat;
			Vector3 pos, startSclSz, endSclSz, sclSz;
			float rot;
			Color col;

			vec3_Lerp( &( renderBuffer[idx].start.pos ), &( renderBuffer[idx].end.pos ), t, &pos );
			vec2ToVec3( &( renderBuffer[idx].start.scaleSize ), 1.0f, &startSclSz );
			vec2ToVec3( &( renderBuffer[idx].end.scaleSize ), 1.0f, &endSclSz );
			col_Lerp( &( renderBuffer[idx].start.color ), &( renderBuffer[idx].end.color ), t, &col );
			vec3_Lerp( &startSclSz, &endSclSz, t, &sclSz );
			rot = lerp( renderBuffer[idx].start.rotation, renderBuffer[idx].end.rotation, t );
			createRenderTransform( &pos, &sclSz, rot, &renderBuffer[idx].offset, &modelTf );

			mat4_Multiply( &vpMat, &modelTf, &mvpMat );

			// set uniforms
			glUniformMatrix4fv( shaderPrograms[SP_SPRITE].uniformLocs[0], 1, GL_FALSE, &( mvpMat.m[0] ) );
			glUniform1i( shaderPrograms[SP_SPRITE].uniformLocs[1], 0 );
			glUniform4fv( shaderPrograms[SP_SPRITE].uniformLocs[2], 1, &( col.col[0] ) );

			// should be sorted by texture object, so only rebind if it's switched
			if( lastBoundTexture != renderBuffer[idx].textureObj ) {
				glBindTexture( GL_TEXTURE_2D, renderBuffer[idx].textureObj );
				lastBoundTexture = renderBuffer[idx].textureObj;
			}

			// and draw
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL );
		}
	}

	if( lastDebugVert >= 0 ) {
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glDisable( GL_BLEND );

		glUseProgram( shaderPrograms[SP_DEBUG].programID );

		glBindVertexArray( debugVAO );

		glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( DebugVertex ) * ( lastDebugVert + 1 ), debugBuffer );

		for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
			unsigned int camFlags = cam_GetFlags( currCamera );
			cam_GetVPMatrix( currCamera, &vpMat );
			glUniformMatrix4fv( shaderPrograms[1].uniformLocs[0], 1, GL_FALSE, &( vpMat.m[0] ) );

			// build the index array and render use it
			int lastDebugIndex = -1;
			for( int i = 0; i < MAX_DEBUG_VERTS; ++i ) {
				if( ( debugBuffer[i].camFlags & camFlags ) != 0 ) {
					++lastDebugIndex;
					debugIndicesBuffer[lastDebugIndex] = i;
				}
			}

			glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, sizeof( GLuint ) * ( lastDebugIndex + 1 ), debugIndicesBuffer );
			glDrawElements( GL_LINES, lastDebugIndex + 1, GL_UNSIGNED_INT, NULL );
		}
	}

	glBindVertexArray( 0 );
	

	glUseProgram( 0 );
}