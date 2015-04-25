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
#include "shaderManager.h"

#include "images.h"
#include "debugRendering.h"

static SDL_GLContext glContext;

static float currentTime;
static float endTime;

static Color clearColor;

/*
Initial setup for the rendering instruction buffer.
 Returns 0 on success.
*/
int gfx_Init( SDL_Window* window )
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

	// init ttf stuff
	if( TTF_Init( ) != 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, TTF_GetError( ) );
		return -1;
	}

	if( img_Init( ) < 0 ) {
		return -1;
	}

	if( debugRenderer_Init( ) < 0 ) {
		return -1;
	}

	clearColor = CLR_MAGENTA;

	return 0;
}

/*
Sets clearing color.
*/
void gfx_SetClearColor( Color newClearColor )
{
	clearColor = newClearColor;
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

	// clear the screen
	glClearColor( clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glDepthMask( GL_TRUE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glClear( GL_COLOR_BUFFER_BIT );

	glDisable( GL_CULL_FACE );

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	// draw all the game play stuff
	img_PreRender( );
	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		glClear( GL_DEPTH_BUFFER_BIT );
		img_Render( currCamera, t );
	}

	// now draw all the debug stuff over everything
	debugRenderer_Render( );
}