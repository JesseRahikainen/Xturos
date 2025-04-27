#ifdef OPENGL_GFX
#ifndef GL_PLATFORM
#define GL_PLATFORM

#include <SDL3/SDL_video.h>

/*
Handles the platform specific OpenGL stuff.
*/
#if defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ ) || defined( __IPHONEOS__ )
  #if defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
    #include <GLES3/gl3.h>
    #if defined( __EMSCRIPTEN__ )
	  #include <GLES2/gl2ext.h>
    #else
	  #include <GLES3/gl3ext.h>
    #endif
  #else // ios
    #include <OpenGLES/ES3/gl.h>
    #include <OpenGLES/ES3/glext.h>
  #endif

#define PROFILE SDL_GL_CONTEXT_PROFILE_ES

#define GL_DEPTH_COMPONENT32 GL_DEPTH_COMPONENT32F
#define GL_BACK_LEFT GL_BACK

#elif defined( WIN32 )
#include "Others/gl_core.h"
#include <SDL3/SDL_opengl.h>

#define PROFILE SDL_GL_CONTEXT_PROFILE_CORE
#endif

int glInit( void );

#endif // inclusion guard
#endif // OPENGL_GFX
