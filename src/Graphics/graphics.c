#include "graphics.h"

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
#include "spineGfx.h"
#include "triRendering.h"

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
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glClear( GL_COLOR_BUFFER_BIT );

	spine_UpdateInstances( dt );
	
	// draw all the stuff that routes through the triangle rendering
	triRenderer_Clear( );
		img_Render( t );
		spine_RenderInstances( t );
	triRenderer_Render( );

	// now draw all the debug stuff over everything
	debugRenderer_Render( );
}