#ifdef OPENGL_GFX
#ifndef GL_PLATFORM
#define GL_PLATFORM

#include <SDL_video.h>

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

// some default shaders
#define DEFAULT_VERTEX_SHADER \
	"#version 300 es\n" \
	"uniform mat4 transform; // view projection matrix\n" \
	"layout(location = 0) in vec3 vVertex;\n" \
	"layout(location = 1) in vec2 vTexCoord0;\n" \
	"layout(location = 2) in vec4 vColor;\n" \
	"out vec2 vTex;\n" \
	"out vec4 vCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vTex = vTexCoord0;\n" \
	"	vCol = vColor;\n" \
	"	gl_Position = transform * vec4( vVertex, 1.0f );\n" \
	"}\n"

#define DEFAULT_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = texture(textureUnit0, vTex) * vCol;\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define FONT_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vec4( vCol.r, vCol.g, vCol.b, texture(textureUnit0, vTex).a * vCol.a );\n" \
	"	if( outCol.w <= 0.0f ) {\n" \
	"		discard;\n" \
	"	}\n" \
	"}\n"

#define DEBUG_VERT_SHADER \
	"#version 300 es\n" \
	"uniform mat4 transform; // model view projection matrix\n" \
	"layout(location = 0) in vec4 vertex;\n" \
	"layout(location = 2) in vec4 color;\n" \
	"out vec4 vertCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vertCol = color;\n" \
	"	gl_Position = transform * vertex;\n" \
	"}\n"

#define DEBUG_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec4 vertCol;\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	outCol = vertCol;\n" \
	"}\n"

/* gotten from here: https://metalbyexample.com/rendering-text-in-metal-with-signed-distance-fields/ */
#define SIMPLE_SDF_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	mediump float dist = texture( textureUnit0, vTex ).a;\n" \
	"	const mediump float edgeDist = 0.5f;\n" \
	"	//float edgeWidth = 0.7f * length( vec2( dFdx( dist ), dFdy( dist ) ) );\n" \
	"	mediump float edgeWidth = 0.7f * fwidth( dist );\n" \
	"	mediump float opacity = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"	outCol = vec4( vCol.r, vCol.g, vCol.b, opacity );\n" \
	"}\n"

// same as the simple SDF fragment shader but uses the alpha, will not support transparency in the image
#define IMAGE_SDF_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"uniform mediump float floatVal0;\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture(textureUnit0, vTex) * vCol;\n" \
	"	mediump float dist = outCol.a;\n" \
	"	const mediump float edgeDist = 0.5f;\n" \
	"	mediump float edgeWidth = 0.7f * fwidth( dist );\n" \
	"   //outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"   if( outCol.a <= 0.0f ) {\n" \
	"	   mediump float t = abs( dist / 0.5f ) * 2.0f;\n" \
	"	   outCol.a = floatVal0 * pow( t, floatVal0 * 5.0f ) * 0.2f;\n" \
	"   }\n" \
	"}\n"

// draws the image using the SDF method, but defines a certain range as an outline, that will be
//  a secondary color
#define OUTLINED_IMAGE_SDF_FRAG_SHADER \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol; // base color\n" \
	"uniform sampler2D textureUnit0;\n" \
	"uniform mediump float floatVal0; // used for outline range\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture(textureUnit0, vTex);\n" \
	"	outCol.a *= vCol.a;\n" \
	"	mediump float dist = outCol.a;\n" \
	"	const mediump float edgeDist = 0.5f;\n" \
	"	mediump float edgeWidth = 0.7f * fwidth( dist );\n" \
	"   //outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"	mediump float outlineEdge = edgeDist + floatVal0;\n" \
	"	mediump float outline = smoothstep( outlineEdge - edgeWidth, outlineEdge + edgeWidth, dist );\n" \
    "   const mediump vec3 white = vec3( 1.0f, 1.0f, 1.0f );\n" \
    "   outCol.rgb = mix( white, vCol.rgb, outline );\n" \
	"}\n"


#define ALPHA_MAPPED_SDF_FRAG_SHADE \
	"#version 300 es\n" \
	"in mediump vec2 vTex;\n" \
	"in mediump vec4 vCol;\n" \
	"uniform sampler2D textureUnit0; // image color\n" \
	"uniform sampler2D textureUnit1; // image sdf alpha mask\n" \
	"uniform mediump float floatVal0; // used for glow strength\n" \
	"out mediump vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture(textureUnit0, vTex) * vCol;\n" \
    "   mediump float dist = texture(textureUnit1, vTex).a;\n " \
	"	const mediump float edgeDist = 0.5f;\n" \
	"	mediump float edgeWidth = 0.7f * fwidth( dist );\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"}\n"

#elif defined( WIN32 )
#include "Others/gl_core.h"
#include <SDL_opengl.h>

#define PROFILE SDL_GL_CONTEXT_PROFILE_CORE

// some default shaders
#define DEFAULT_VERTEX_SHADER \
	"#version 330\n" \
	"uniform mat4 transform; // view projection matrix\n" \
	"layout(location = 0) in vec3 vVertex;\n" \
	"layout(location = 1) in vec2 vTexCoord0;\n" \
	"layout(location = 2) in vec4 vColor;\n" \
	"out vec2 vTex;\n" \
	"out vec4 vCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vTex = vTexCoord0;\n" \
	"	vCol = vColor;\n" \
	"	gl_Position = transform * vec4( vVertex, 1.0f );\n" \
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
	"   //outCol = vCol;\n" \
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

/* gotten from here: https://metalbyexample.com/rendering-text-in-metal-with-signed-distance-fields/ */
#define SIMPLE_SDF_FRAG_SHADER \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	float dist = texture2D( textureUnit0, vTex ).r;\n" \
	"	float edgeDist = 0.5f;\n" \
	"	//float edgeWidth = 0.7f * length( vec2( dFdx( dist ), dFdy( dist ) ) );\n" \
	"	float edgeWidth = 0.7f * fwidth( dist );\n" \
	"	float opacity = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"	outCol = vec4( vCol.r, vCol.g, vCol.b, opacity );\n" \
	"}\n"

// same as the simple SDF fragment shader but uses the alpha, will not support transparency in the image
//  TODO: Make the SDF based on a separate mask image
#define IMAGE_SDF_FRAG_SHADER \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol;\n" \
	"uniform sampler2D textureUnit0;\n" \
	"uniform float floatVal0; // used for glow strength\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture2D(textureUnit0, vTex) * vCol;\n" \
	"	float dist = outCol.a;\n" \
	"	float edgeDist = 0.5f;\n" \
	"	float edgeWidth = 0.7f * fwidth( dist );\n" \
	"   //outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"   if( outCol.a <= 0.0f ) {\n" \
	"	   float t = abs( dist / 0.5f ) * 2.0f;\n" \
	"	   outCol.a = floatVal0 * pow( t, floatVal0 * 5.0f ) * 0.2f;\n" \
	"   }\n" \
	"}\n"

// draws the image using the SDF method, but defines a certain range as an outline, the outline
//  will be assumed to be white
#define OUTLINED_IMAGE_SDF_FRAG_SHADER \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol; // base color\n" \
	"uniform sampler2D textureUnit0;\n" \
	"uniform float floatVal0; // used for outline range\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture2D(textureUnit0, vTex);\n" \
	"	outCol.a *= vCol.a;\n" \
	"	float dist = outCol.a;\n" \
	"	float edgeDist = 0.5f;\n" \
	"	float edgeWidth = 0.7f * fwidth( dist );\n" \
	"   //outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"	float outlineEdge = edgeDist + floatVal0;\n" \
	"	float outline = smoothstep( outlineEdge - edgeWidth, outlineEdge + edgeWidth, dist );\n" \
    "   outCol.rgb = mix( vec3( 1.0f, 1.0f, 1.0f ), vCol.rgb, outline );\n" \
	"}\n"

#define ALPHA_MAPPED_SDF_FRAG_SHADE \
	"#version 330\n" \
	"in vec2 vTex;\n" \
	"in vec4 vCol;\n" \
	"uniform sampler2D textureUnit0; // image color\n" \
	"uniform sampler2D textureUnit1; // image sdf alpha mask\n" \
	"uniform float floatVal0; // used for glow strength\n" \
	"out vec4 outCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"   outCol = texture2D(textureUnit0, vTex) * vCol;\n" \
    "   float dist = texture2D(textureUnit1, vTex).a;\n " \
	"	float edgeDist = 0.5f;\n" \
	"	float edgeWidth = 0.7f * fwidth( dist );\n" \
	"	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );\n" \
	"}\n"

#define DEBUG_VERT_SHADER \
	"#version 330\n" \
	"uniform mat4 transform; // model view projection matrix\n" \
	"layout(location = 0) in vec4 vertex;\n" \
	"layout(location = 2) in vec4 color;\n" \
	"out vec4 vertCol;\n" \
	"void main( void )\n" \
	"{\n" \
	"	vertCol = color;\n" \
	"	gl_Position = transform * vertex;\n" \
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
#endif // OPENGL_GFX
