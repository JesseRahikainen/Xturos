#ifdef OPENGL_GFX

#include <SDL_syswm.h>
#include <SDL_video.h>

#include "Graphics/Platform/graphicsPlatform.h"

#include <stb_image.h>

#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/Platform/OpenGL/glDebugging.h"
#include "System/platformLog.h"
#include "System/memory.h"

#include "Graphics/graphics.h"

// renderWidth, renderHeight, and windowClearColor all need to be in some sort of shared file

// implementation for OpenGL
static SDL_GLContext glContext;

// need to pass in the pointer to the value since we have to use glDrawBuffers
static GLenum screenBuffer = GL_BACK_LEFT;

static enum {
	COLOR_RBO,
	DEPTH_RBO,
	RBO_COUNT
} MainRBO;

static GLuint mainRenderFBO = 0;
static GLuint mainRenderRBOs[RBO_COUNT] = { 0, 0 };

static GLuint defaultFBO = 0;
static GLuint defaultColorRBO = 0;

static void destroyFBO( GLuint* fboOut, GLuint* rbosOut )
{
	GL( glDeleteFramebuffers( 1, fboOut ) );
	( *fboOut ) = 0;

	GL( glDeleteRenderbuffers( RBO_COUNT, rbosOut ) );
	memset( rbosOut, 0, sizeof( rbosOut[0] ) * RBO_COUNT );
}

static int generateFBO( int renderWidth, int renderHeight, GLuint* fboOut, GLuint* rbosOut )
{
	GL( glGenFramebuffers( 1, fboOut ) );
	GL( glGenRenderbuffers( RBO_COUNT, rbosOut ) );

	//  create the render buffer objects
	GL( glBindRenderbuffer( GL_RENDERBUFFER, rbosOut[DEPTH_RBO] ) );
	//GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, renderWidth, renderHeight ) );/*
	GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, renderWidth, renderHeight ) );//*/

	GL( glBindRenderbuffer( GL_RENDERBUFFER, rbosOut[COLOR_RBO] ) );
	GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_RGB8, renderWidth, renderHeight ) );

	//  bind the render buffer objects
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, ( *fboOut ) ) );
	GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbosOut[COLOR_RBO] ) );
	//GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbosOut[DEPTH_RBO] ) );/*
	GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbosOut[DEPTH_RBO] ) );//*/

	if( checkAndLogFrameBufferCompleteness( GL_DRAW_FRAMEBUFFER, NULL ) < 0 ) {
		return -1;
	}

	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );

	return 0;
}

bool gfxPlatform_Init( SDL_Window* window, int desiredRenderWidth, int desiredRenderHeight )
{
	// setup opengl
	glContext = SDL_GL_CreateContext( window );
	if( glContext == NULL ) {
		llog( LOG_ERROR, "Error in initRendering while creating context: %s", SDL_GetError( ) );
		return -1;
	}
	SDL_GL_MakeCurrent( window, glContext );
	llog( LOG_INFO, "OpenGL context created." );

	// initialize opengl
	if( glInit( ) < 0 ) {
		return -1;
	}
	llog( LOG_INFO, "OpenGL initialized." );

	// use v-sync, avoid tearing
	if( SDL_GL_SetSwapInterval( 1 ) < 0 ) {
		llog( LOG_INFO, "Unable to set vertical retrace swap: %s", SDL_GetError( ) );
		
        if( SDL_GL_SetSwapInterval( -1 ) < 0 ) {
            llog( LOG_INFO, "Unable to set adaptive swap: %s", SDL_GetError( ) );
        }
	}

	// set packing and unpacking to 1 byte alignment, works better with our font textures
	GL( glPixelStorei( GL_PACK_ALIGNMENT, 1 ) );
	GL( glPixelStorei( GL_UNPACK_ALIGNMENT, 1 ) );

	int finalWidth, finalHeight;
	gfx_calculateRenderSize( desiredRenderWidth, desiredRenderHeight, &finalWidth, &finalHeight );

	//llog( LOG_DEBUG, "render size: %i x %i", finalWidth, finalHeight );

	// create the main render frame buffer object
	if( generateFBO( finalWidth, finalHeight, &mainRenderFBO, &( mainRenderRBOs[0] ) ) < 0 ) {
		return false;
	}
    
#ifdef __IPHONEOS__
    // iOS needs the default framebuffer and colorbuffer
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( !SDL_GetWindowWMInfo( window, &info ) ) {
        llog( LOG_ERROR, "Unable to retrive window information: %s", SDL_GetError( ) );
        return -1;
    }
    
    if( info.subsystem != SDL_SYSWM_UIKIT ) {
        llog( LOG_ERROR, "Invalid subsystem type." );
        return -1;
    }
    
    defaultFBO = info.info.uikit.framebuffer;
    defaultColorRBO = info.info.uikit.colorbuffer;
#endif

	return true;
}

int gfxPlatform_GetMaxSize( int desiredSize )
{
	GLint maxSize;
	GL( glGetIntegerv( GL_MAX_RENDERBUFFER_SIZE, &maxSize ) );

	return (int)maxSize;
}

void gfxPlatform_DynamicSizeRender( float dt, float t, int renderWidth, int renderHeight,
	int windowRenderX0, int windowRenderY0, int windowRenderX1, int windowRenderY1, Color clearColor )
{
	GL( glViewport( 0, 0, renderWidth, renderHeight ) );

#if !defined( __IPHONEOS__ ) && !defined( __ANDROID__ ) && !defined( __EMSCRIPTEN__ )
	GL( glEnable( GL_MULTISAMPLE ) );
#endif

	// draw the game stuff
	GLenum mainRenderBuffers = GL_COLOR_ATTACHMENT0;
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, mainRenderFBO ) );
        GL( glDrawBuffers( 1, &mainRenderBuffers ) );
        // clear the screen and draw everything to a frame buffer
        GL( glClearColor( clearColor.r, clearColor.g, clearColor.b, clearColor.a ) );
        GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
		GL( glDepthMask( true ) );
        GL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );
		gfx_MakeRenderCalls( dt, t );
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, defaultFBO ) );

	// now render the main frame buffer to the screen, scaling based on the size of the window
	GL( glBindFramebuffer( GL_READ_FRAMEBUFFER, mainRenderFBO ) );
		GL( glReadBuffer( GL_COLOR_ATTACHMENT0 ) );
		GL( glBlitFramebuffer( 0, 0, renderWidth, renderHeight,
			windowRenderX0, windowRenderY0, windowRenderX1, windowRenderY1,
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR ) );
	GL( glBindFramebuffer( GL_READ_FRAMEBUFFER, defaultFBO ) );

    
    // iOS: need to make sure the framebuffer and colorbuffer objects in the SysWMInfo are correctly bound
    //  framebuffer should already be bound with the defaultFBO command above
#ifdef __IPHONEOS__
    GL( glBindRenderbuffer( GL_RENDERBUFFER, defaultColorRBO ) );
#endif
    
	// editor and debugging ui stuff
	//nk_xu_render( &editorIMGUI );

#if !defined( __IPHONEOS__ ) && !defined( __ANDROID__ ) && !defined( __EMSCRIPTEN__ )
	GL( glDisable( GL_MULTISAMPLE ) );
#endif
}

void gfxPlatform_StaticSizeRender( float dt, float t, Color clearColor )
{
	// clear the screen
	GL( glClearColor( clearColor.r, clearColor.g, clearColor.b, clearColor.a ) );
	GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );

	gfx_MakeRenderCalls( dt, t );
}

void gfxPlatform_RenderResize( int newDesiredRenderWidth, int newDesiredRenderHeight )
{
	destroyFBO( &mainRenderFBO, &( mainRenderRBOs[0] ) );
	generateFBO( newDesiredRenderWidth, newDesiredRenderHeight, &mainRenderFBO, &( mainRenderRBOs[0] ) );
}

void gfxPlatform_CleanUp( void )
{
	GL( glDeleteRenderbuffers( RBO_COUNT, &( mainRenderRBOs[0] ) ) );
	GL( glDeleteFramebuffers( 1, &mainRenderFBO ) );
}

void gfxPlatform_ShutDown( void )
{
	SDL_GL_DeleteContext( glContext );
}

bool gfxPlatform_CreateTextureFromLoadedImage( TextureFormat texFormat, LoadedImage* image, Texture* outTexture )
{
	int start = 3;
	int step = 4;
    GLenum glTexFormat = GL_RGBA;
    switch( texFormat ) {
        case TF_RED:
			start = 0;
			step = 1;
            glTexFormat = GL_RED;
            break;
        case TF_GREEN:
			start = 0;
			step = 1;
            glTexFormat = GL_GREEN;
            break;
        case TF_BLUE:
			start = 0;
			step = 1;
            glTexFormat = GL_BLUE;
            break;
        case TF_ALPHA:
			start = 0;
			step = 1;
            glTexFormat = GL_ALPHA;
            break;
        case TF_RGBA:
			start = 3;
			step = 4;
            glTexFormat = GL_RGBA;
			break;
    }
    
	GL( glGenTextures( 1, &( outTexture->texture.id ) ) );

	if( outTexture->texture.id == 0 ) {
		llog( LOG_INFO, "Unable to create texture object." );
		return false;
	}

	GL( glBindTexture( GL_TEXTURE_2D, outTexture->texture.id ) );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
	// TODO: Make this configurable? Possible somehow tied into the material?
	//GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ) );
	//GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ) );

	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

	GL( glTexImage2D( GL_TEXTURE_2D, 0, glTexFormat, image->width, image->height, 0, glTexFormat, GL_UNSIGNED_BYTE, image->data ) );

	outTexture->width = image->width;
	outTexture->height = image->height;
	outTexture->flags = 0;

	// check to see if there are any translucent pixels in the image
	for( int i = start; ( i < ( image->width * image->height ) ) && !( outTexture->flags & TF_IS_TRANSPARENT ); i += step ) {
		if( ( image->data[i] > 0x00 ) && ( image->data[i] < 0xFF ) ) {
			outTexture->flags |= TF_IS_TRANSPARENT;
		}
	}

	return true;
}

bool gfxPlatform_CreateTextureFromSurface( SDL_Surface* surface, Texture* outTexture )
{
	// convert the pixels into a texture
	GLenum texFormat;

	if( surface->format->BytesPerPixel == 4 ) {
		texFormat = GL_RGBA;
	} else if( surface->format->BytesPerPixel == 3 ) {
		texFormat = GL_RGB;
	} else {
		llog( LOG_INFO, "Unable to handle format!" );
		return false;
	}

	// TODO: Get this working with createTextureFromLoadedImage( ), just need to make an SDL_Surface into a LoadedImage
	GL( glGenTextures( 1, &( outTexture->texture.id ) ) );

	if( outTexture->texture.id == 0 ) {
		llog( LOG_INFO, "Unable to create texture object." );
		return false;
	}

	GL( glBindTexture( GL_TEXTURE_2D, outTexture->texture.id ) );

	// assuming these will look good for now, we shouldn't be too much resizing, but if we do we can go over these again
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );

	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
	GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

	GL( glTexImage2D( GL_TEXTURE_2D, 0, texFormat, surface->w, surface->h, 0, texFormat, GL_UNSIGNED_BYTE, surface->pixels ) );

	outTexture->width = surface->w;
	outTexture->height = surface->h;
	outTexture->flags = 0;
	// the reason this isn't using the createTextureFromLoadedImage is this primarily
	if( gfxUtil_SurfaceIsTranslucent( surface ) ) {
		outTexture->flags |= TF_IS_TRANSPARENT;
	}

	return true;
}

void gfxPlatform_UnloadTexture( Texture* texture )
{
	GL( glDeleteTextures( 1, &( texture->texture.id ) ) );
	texture->texture.id = 0;
	texture->flags = 0;
}

int gfxPlatform_ComparePlatformTextures( PlatformTexture lhs, PlatformTexture rhs )
{
	if( lhs.id < rhs.id ) {
		return -1;
	} else if( lhs.id > rhs.id ) {
		return 1;
	}
	return 0;
}

void gfxPlatform_DeletePlatformTexture( PlatformTexture texture )
{
	glDeleteTextures( 1, &( texture.id ) );
}

void gfxPlatform_Swap( SDL_Window* window )
{
    SDL_GL_SwapWindow( window );
}

uint8_t* gfxPlatform_GetScreenShotPixels( int width, int height )
{
	uint8_t* pixels = mem_Allocate( sizeof( uint8_t ) * width * height * 3 );
	glReadPixels( 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels );

	// OGL stores the buffer flipped vertically, so we need to flip it to get it right
	size_t rowSizeBytes = sizeof( uint8_t ) * width * 3;
	uint8_t* pixelRow = mem_Allocate( rowSizeBytes );

	for( int i = 0; i < ( height / 2 ); ++i ) {
		int row0 = i * width * 3;
		int row1 = ( height - i - 1 ) * width * 3;
		memcpy( pixelRow, pixels + row0, rowSizeBytes );
		memcpy( pixels + row0, pixels + row1, rowSizeBytes );
		memcpy( pixels + row1, pixelRow, rowSizeBytes );
	}

	mem_Release( pixelRow );

	return pixels;
}

int gfxPlatform_GetMaxTextureSize( void )
{
    GLint maxTextureSize;
    glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );
    return (int)maxTextureSize;
}

PlatformTexture gfxPlatform_GetDefaultPlatformTexture( void )
{
	PlatformTexture pt;
	pt.id = 0;
	return pt;
}
#endif
