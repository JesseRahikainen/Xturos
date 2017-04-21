#ifndef GL_PLATFORM
#define GL_PLATFORM

#include <SDL_video.h>

/*
Handles the platform specific OpenGL stuff.
*/
#if defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )

#include <GLES3/gl3.h>
#if defined( __EMSCRIPTEN__ )
	#include <GLES2/gl2ext.h>
#else
	#include <GLES3/gl3ext.h>
#endif

#define PROFILE SDL_GL_CONTEXT_PROFILE_ES

#define GL_DEPTH_COMPONENT32 GL_DEPTH_COMPONENT32F
#define GL_BACK_LEFT GL_BACK

// some default shaders
#define DEFAULT_VERTEX_SHADER \
	"#version 300 es\n" \
	"uniform mat4 vpMatrix;\n" \
	"layout(location = 0) in vec3 vVertex;\n" \
	"layout(location = 1) in vec2 vTexCoord0;\n" \
	"layout(location = 2) in vec4 vColor;\n" \
	"out vec2 vTex;\n" \
	"out vec4 vCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vTex = vTexCoord0;\n" \
	"	vCol = vColor;\n" \
	"	gl_Position = vpMatrix * vec4( vVertex, 1.0f );\n" \
	"}\n"

#define DEFAULT_FRAG_SHADER \
	"#version 300 es\n" \
	"in highp vec2 vTex;\n" \
	"in highp vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out highp vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = texture(textureUnit0, vTex) * vCol;\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define FONT_FRAG_SHADER \
	"#version 300 es\n" \
	"in highp vec2 vTex;\n" \
	"in highp vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out highp vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vec4( vCol.r, vCol.g, vCol.b, texture(textureUnit0, vTex).a * vCol.a );\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define DEBUG_VERT_SHADER \
	"#version 300 es\n" \
	"uniform mat4 mvpMatrix;\n" \
	"layout(location = 0) in vec4 vertex;\n" \
	"layout(location = 2) in vec4 color;\n" \
	"out vec4 vertCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vertCol = color;\n" \
	"	gl_Position = mvpMatrix * vertex;\n" \
	"}\n"

#define DEBUG_FRAG_SHADER \
	"#version 300 es\n" \
	"in highp vec4 vertCol;\n" \
	"out highp vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vertCol;\n" \
	"}\n"

#elif defined( WIN32 )
#include "../Others/gl_core.h"
#include <SDL_opengl.h>

#define PROFILE SDL_GL_CONTEXT_PROFILE_CORE

// some default shaders
#define DEFAULT_VERTEX_SHADER \
	"#version 330\n" \
	"uniform mat4 vpMatrix;\n" \
	"layout(location = 0) in vec3 vVertex;\n" \
	"layout(location = 1) in vec2 vTexCoord0;\n" \
	"layout(location = 2) in vec4 vColor;\n" \
	"out vec2 vTex;\n" \
	"out vec4 vCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vTex = vTexCoord0;\n" \
	"	vCol = vColor;\n" \
	"	gl_Position = vpMatrix * vec4( vVertex, 1.0f );\n" \
	"}\n"

#define DEFAULT_FRAG_SHADER \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = texture2D(textureUnit0, vTex) * vCol;\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define FONT_FRAG_SHADER \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vec4( vCol.r, vCol.g, vCol.b, texture2D(textureUnit0, vTex).r * vCol.a );\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define DEBUG_VERT_SHADER \
	"#version 330\n" \
	"uniform mat4 mvpMatrix;\n" \
	"layout(location = 0) in vec4 vertex;\n" \
	"layout(location = 2) in vec4 color;\n" \
	"out vec4 vertCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vertCol = color;\n" \
	"	gl_Position = mvpMatrix * vertex;\n" \
	"}\n"

#define DEBUG_FRAG_SHADER \
	"#version 330\n" \
	"in vec4 vertCol;\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vertCol;\n" \
	"}\n"
#endif

int glInit( void );

#endif // inclusion guard