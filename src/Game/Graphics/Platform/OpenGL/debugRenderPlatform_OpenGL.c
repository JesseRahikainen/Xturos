#ifdef OPENGL_GFX

#include "Graphics/Platform/debugRenderingPlatform.h"

#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/Platform/OpenGL/glShaderManager.h"
#include "Graphics/Platform/OpenGL/glDebugging.h"
#include "System/platformLog.h"
#include "Math/matrix4.h"
#include "Graphics/camera.h"

static GLuint debugVAO;
static GLuint debugVBO;
static GLuint debugIBO;

static ShaderProgram debugShaderProgram;

static GLuint debugIndicesBuffer[MAX_DEBUG_VERTS];

static int createOGLObjects( size_t bufferSize )
{
	GL( glGenBuffers( 1, &debugVBO ) );
	GL( glGenBuffers( 1, &debugIBO ) );
	GL( glGenVertexArrays( 1, &debugVAO ) );
	if( ( debugVBO == 0 ) || ( debugIBO == 0 ) || ( debugVAO == 0 ) ) {
		llog( LOG_ERROR, "Unable to create one or more storage objects for debug rendering." );
		return -1;
	}

	// reserver space for the buffers
	GL( glBindVertexArray( debugVAO ) );

	GL( glBindBuffer( GL_ARRAY_BUFFER, debugVBO ) );
	GL( glBufferData( GL_ARRAY_BUFFER, bufferSize, NULL, GL_DYNAMIC_DRAW ) );

	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, debugIBO ) );
	GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( debugIndicesBuffer ), debugIndicesBuffer, GL_DYNAMIC_DRAW ) );

	GL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( DebugVertex ), (const GLvoid*)offsetof( DebugVertex, pos ) ) );
	GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( DebugVertex ), (const GLvoid*)offsetof( DebugVertex, color ) ) );

	GL( glEnableVertexAttribArray( 0 ) );
	GL( glEnableVertexAttribArray( 2 ) );

	GL( glBindVertexArray( 0 ) );

	return 0;
}

static int createShader( void )
{
	ShaderDefinition debugShaderDefs[2];
	ShaderProgramDefinition debugProgDef;
	debugShaderDefs[0].fileName = NULL;
	debugShaderDefs[0].type = GL_VERTEX_SHADER;
	debugShaderDefs[0].shaderText = DEBUG_VERT_SHADER;

	debugShaderDefs[1].fileName = NULL;
	debugShaderDefs[1].type = GL_FRAGMENT_SHADER;
	debugShaderDefs[1].shaderText = DEBUG_FRAG_SHADER;

	debugProgDef.fragmentShader = 1;
	debugProgDef.vertexShader = 0;
	debugProgDef.geometryShader = -1;

	if( shaders_Load( &( debugShaderDefs[0] ), sizeof( debugShaderDefs ) / sizeof( ShaderDefinition ),
		&debugProgDef, &debugShaderProgram, 1 ) <= 0 ) {
		llog( LOG_INFO, "Error compiling debug shaders.\n" );
		return -1;
	}

	return 0;
}

bool debugRendererPlatform_Init( size_t bufferSize )
{
	if( createOGLObjects( bufferSize ) < 0 ) {
		return false;
	}

	if( createShader( ) < 0 ) {
		return false;
	}

	return true;
}

void debugRendererPlatform_Render( DebugVertex* debugBuffer, int lastDebugVert )
{
	Matrix4 vpMat;
	if( lastDebugVert >= 0 ) {
		GL( glDisable( GL_DEPTH_TEST ) );
		GL( glDepthMask( GL_FALSE ) );
		GL( glDisable( GL_BLEND ) );
		GL( glDisable( GL_STENCIL_TEST ) );

		GL( glUseProgram( debugShaderProgram.programID ) );
		GL( glBindVertexArray( debugVAO ) );

		// using glGetIntegerv with GL_ARAY_BUFFER_BINDING returns the correct buffer name
		//  but glBufferSubData will fail, the buffer is not mapped, so it seems like the
		//  array buffer is in some sort of limbo
		// so we shouldn't have to do this, but we do
		// or i don't understand OpenGL as well as i think i do (most likely)
		GL( glBindBuffer( GL_ARRAY_BUFFER, debugVBO ) );
		GL( glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( DebugVertex ) * ( lastDebugVert + 1 ), debugBuffer ) );

		for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
			unsigned int camFlags = cam_GetFlags( currCamera );
			cam_GetVPMatrix( currCamera, &vpMat );
			GL( glUniformMatrix4fv( debugShaderProgram.uniformLocs[0], 1, GL_FALSE, &( vpMat.m[0] ) ) );

			// build the index array and render it
			int lastDebugIndex = -1;
			for( int i = 0; i <= lastDebugVert; ++i ) {
				if( ( debugBuffer[i].camFlags & camFlags ) != 0 ) {
					++lastDebugIndex;
					debugIndicesBuffer[lastDebugIndex] = i;
				}
			}

			GL( glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, sizeof( GLuint ) * ( lastDebugIndex + 1 ), debugIndicesBuffer ) );
			GL( glDrawElements( GL_LINES, lastDebugIndex + 1, GL_UNSIGNED_INT, NULL ) );
		}

		GL( glBindVertexArray( 0 ) );
		GL( glUseProgram( 0 ) );
	}
}

#endif
