#include "graphics.h"

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../Others/glew.h"
#include <SDL_opengl.h>

#include "glDebugging.h"

#include "camera.h"
#include "../Math/MathUtil.h"
#include "shaderManager.h"

#include "images.h"
#include "debugRendering.h"
#include "spineGfx.h"
#include "triRendering.h"

static SDL_GLContext glContext;

static float currentTime;
static float endTime;

// both the clear colors default to black
static Color gameClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
static Color windowClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

// defines the size of the actual area we'll be rendering to
static int renderWidth;
static int renderHeight;

// defines where in the window we'll copy the rendered area to
static int windowRenderX0;
static int windowRenderX1;
static int windowRenderY0;
static int windowRenderY1;

static enum {
	COLOR_RBO,
	DEPTH_RBO,
	RBO_COUNT
} MainRBO;

static GLuint mainRenderFBO = 0;
static GLuint mainRenderRBOs[RBO_COUNT] = { 0, 0 };

/*
Initial setup for the rendering instruction buffer.
 Returns 0 on success.
*/
int gfx_Init( SDL_Window* window, int desiredRenderWidth, int desiredRenderHeight )
{
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

	// create the main render frame buffer object
	GLint maxSize;
	GL( glGetIntegerv( GL_MAX_RENDERBUFFER_SIZE, &maxSize ) );

	renderWidth = desiredRenderWidth;
	renderHeight = desiredRenderHeight;

	float ratio = (float)renderWidth/(float)renderHeight;
	if( renderWidth > maxSize ) {
		renderWidth = (int)maxSize;
		renderHeight = (int)( maxSize / ratio );
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Render width outside maximum size allowed by OpenGL, resizing to %ix%i", renderWidth, renderHeight );
	} else if( renderHeight > maxSize ) {
		renderWidth = (int)( maxSize * ratio );
		renderHeight = (int)maxSize;
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Render height outside maximum size allowed by OpenGL, resizing to %ix%i", renderWidth, renderHeight );
	}
	GL( glGenFramebuffers( 1, &mainRenderFBO ) );
	GL( glGenRenderbuffers( RBO_COUNT, &( mainRenderRBOs[0] ) ) );

	//  create the render buffer objects
	GL( glBindRenderbuffer( GL_RENDERBUFFER, mainRenderRBOs[DEPTH_RBO] ) );
	//GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, renderWidth, renderHeight ) ); // this doesn't work for some reason...
	//GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, renderWidth, renderHeight ) );
	GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, renderWidth, renderHeight ) );
	
	GL( glBindRenderbuffer( GL_RENDERBUFFER, mainRenderRBOs[COLOR_RBO] ) );
	GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_RGB8, renderWidth, renderHeight ) );

	//  bind the render buffer objects
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, mainRenderFBO ) );
	GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mainRenderRBOs[DEPTH_RBO] ) );
	GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mainRenderRBOs[COLOR_RBO] ) );

	if( checkAndLogFrameBufferCompleteness( GL_DRAW_FRAMEBUFFER, NULL ) < 0 ) {
		return -1;
	}

	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );

	int windowWidth, windowHeight;
	SDL_GetWindowSize( window, &windowWidth, &windowHeight );
	gfx_SetWindowSize( windowWidth, windowHeight );

	// initialize everything else
	if( img_Init( ) < 0 ) {
		return -1;
	}

	if( debugRenderer_Init( ) < 0 ) {
		return -1;
	}

	spine_Init( );

	if( triRenderer_Init( ) < 0 ) {
		return -1;
	}

	gameClearColor = CLR_MAGENTA;

	return 0;
}

void gfx_SetWindowSize( int windowWidth, int windowHeight )
{
	int centerX = windowWidth / 2;
	int centerY = windowHeight / 2;

	int width;
	int height;

	float windowRatio = (float)windowWidth/(float)windowHeight;
	float renderRatio = (float)renderWidth/(float)renderHeight;

	if( windowRatio < renderRatio ) {
		windowRenderX0 = 0;
		windowRenderX1 = windowWidth;

		height = (int)( windowWidth / renderRatio );
		windowRenderY0 = centerY - ( height / 2 );
		windowRenderY1 = centerY + ( height / 2 );
	} else {
		width = (int)( windowHeight * renderRatio );
		windowRenderX0 = centerX - ( width / 2 );
		windowRenderX1 = centerX + ( width / 2 );

		windowRenderY0 = 0;
		windowRenderY1 = windowHeight;
	}
}

void gfx_CleanUp( void )
{
	GL( glDeleteRenderbuffers( RBO_COUNT, &( mainRenderRBOs[0] ) ) ); 
	GL( glDeleteFramebuffers( 1, &mainRenderFBO ) );
}

/*
Sets clearing color.
*/
void gfx_SetClearColor( Color newClearColor )
{
	gameClearColor = newClearColor;
}

/*
Sets the non render area clearing color.
*/
void gfx_SetWindowClearColor( Color newClearColor )
{
	windowClearColor = newClearColor;
}

/*
Clears all the drawing instructions.
*/
void gfx_ClearDrawCommands( float timeToEnd )
{
	debugRenderer_ClearVertices( );
	img_ClearDrawInstructions( );
	
	endTime = timeToEnd;
	currentTime = 0.0f;
}

/*
Goes through everything in the render buffer and does the actual rendering.
Resets the render buffer.
*/
void gfx_Render( float dt )
{
	float t;

	currentTime += dt;
	t = clamp( 0.0f, 1.0f, ( currentTime / endTime ) );

	GLenum mainRenderBuffers = GL_COLOR_ATTACHMENT0;
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, mainRenderFBO ) );
		GL( glDrawBuffers( 1, &mainRenderBuffers ) );
		// clear the screen
		GL( glClearColor( gameClearColor.r, gameClearColor.g, gameClearColor.b, gameClearColor.a) );
		GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
		GL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );

		spine_UpdateInstances( dt );
		spine_FlipInstancePositions( );
	
		// draw all the stuff that routes through the triangle rendering
		triRenderer_Clear( );
			img_Render( t );
			spine_RenderInstances( t );
		triRenderer_Render( );

		// now draw all the debug stuff over everything
		debugRenderer_Render( );
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );

	// now render everything to the screen, scaling based on the size of the window
	GL( glDrawBuffer( GL_BACK_LEFT ) );
	GL( glViewport( 0, 0, renderWidth, renderHeight ) );

	GL( glClearColor( windowClearColor.r, windowClearColor.g, windowClearColor.b, windowClearColor.a) );
	GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );

	GL( glBindFramebuffer( GL_READ_FRAMEBUFFER, mainRenderFBO ) );
	GL( glReadBuffer( GL_COLOR_ATTACHMENT0 ) );
	GL( glBlitFramebuffer( 0, 0, renderWidth, renderHeight,
					windowRenderX0, windowRenderY0, windowRenderX1, windowRenderY1,
					GL_COLOR_BUFFER_BIT, GL_LINEAR ) );
	GL( glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 ) );
}