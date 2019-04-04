#include "debugRendering.h"

#include "glPlatform.h"
#include <math.h>

#include "../Math/matrix4.h"
#include "color.h"
#include "camera.h"
#include "shaderManager.h"
#include "glDebugging.h"
#include "../System/platformLog.h"

static GLuint debugVAO;
static GLuint debugVBO;
static GLuint debugIBO;

#define MAX_VERTS 2048
#if MAX_VERTS % 2 == 1
	#error "debugRendering MAX_VERTS needs to be divisible by 2."
#endif

#define NUM_CIRC_VERTS 8

typedef struct {
	Vector3 pos;
	Color color;
	unsigned int camFlags;
} DebugVertex;

static DebugVertex debugBuffer[MAX_VERTS];
static int lastDebugVert;

static GLuint debugIndicesBuffer[MAX_VERTS];

static ShaderProgram debugShaderProgram;

static int createOGLObjects( void )
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
	GL( glBufferData( GL_ARRAY_BUFFER, sizeof( debugBuffer ), NULL, GL_DYNAMIC_DRAW ) );

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
	debugShaderDefs[0].shaderText =	DEBUG_VERT_SHADER;

	debugShaderDefs[1].fileName = NULL;
	debugShaderDefs[1].type = GL_FRAGMENT_SHADER;
	debugShaderDefs[1].shaderText =	DEBUG_FRAG_SHADER;

	debugProgDef.fragmentShader = 1;
	debugProgDef.vertexShader = 0;
	debugProgDef.geometryShader = -1;
	debugProgDef.uniformNames = "mvpMatrix";

	if( shaders_Load( &( debugShaderDefs[0] ), sizeof( debugShaderDefs ) / sizeof( ShaderDefinition ),
		&debugProgDef, &debugShaderProgram, 1 ) <= 0 ) {
		llog( LOG_INFO, "Error compiling debug shaders.\n" );
		return -1;
	}

	return 0;
}

int debugRenderer_Init( void )
{
	// create the opengl objects
	if( createOGLObjects( ) < 0 ) {
		return -1;
	}

	// create the shader program
	if( createShader( ) < 0 ) {
		return -1;
	}

	lastDebugVert = -1;

	return 0;
}

/*
Empties the list of debug things to draw.
*/
void debugRenderer_ClearVertices( void )
{
	lastDebugVert = -1;
}

/*
Some basic debug drawing functions.
  Returns 0 on success. Prints an error message to the log if it fails and returns -1.
*/
static int queueDebugVert( unsigned int camFlags, Vector2 pos, Color color )
{
	if( lastDebugVert >= ( MAX_VERTS - 1 ) ) {
		llog( LOG_VERBOSE, "Debug instruction queue full." );
		return -1;
	}

	++lastDebugVert;
	vec2ToVec3( &pos, 0.0f, &( debugBuffer[lastDebugVert].pos ) );
	debugBuffer[lastDebugVert].color = color;
	debugBuffer[lastDebugVert].color.a = 1.0f;
	debugBuffer[lastDebugVert].camFlags = camFlags;

	return 0;
}

int debugRenderer_AABB( unsigned int camFlags, Vector2 topLeft, Vector2 size, Color color )
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
		fail = fail || debugRenderer_Line( camFlags, corners[i], corners[(i+1)%4], color );
	}

	return -fail;
}

int debugRenderer_Line( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Color color )
{
	if( queueDebugVert( camFlags, pOne, color ) ) {
		return -1;
	}
	if( queueDebugVert( camFlags, pTwo, color ) ) {
		return -1;
	}

	return 0;
}

int debugRenderer_Circle( unsigned int camFlags, Vector2 center, float radius, Color color )
{
	int fail;
	Vector2 circPos[NUM_CIRC_VERTS];

	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		circPos[i].v[0] = center.v[0] + ( sinf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
		circPos[i].v[1] = center.v[1] + ( cosf( ( (float)(i) ) * ( ( 2.0f * (float)M_PI ) / (float)(NUM_CIRC_VERTS) ) ) * radius );
	}

	fail = 0;
	for( int i = 0; i < NUM_CIRC_VERTS; ++i ) {
		fail = fail || debugRenderer_Line( camFlags, circPos[i], circPos[( i + 1 ) % NUM_CIRC_VERTS], color );
	}

	return -fail;
}

int debugRenderer_Triangle( unsigned int camFlags, Vector2 pOne, Vector2 pTwo, Vector2 pThree, Color color )
{
	int fail = 0;
	fail = fail || debugRenderer_Line( camFlags, pOne, pTwo, color );
	fail = fail || debugRenderer_Line( camFlags, pOne, pThree, color );
	fail = fail || debugRenderer_Line( camFlags, pThree, pTwo, color );

	return -fail;
}

/*
Draw all the debug lines.
*/
void debugRenderer_Render( void )
{
	Matrix4 vpMat;

	if( lastDebugVert >= 0 ) {
		GL( glDisable( GL_DEPTH_TEST ) );
		GL( glDepthMask( GL_FALSE ) );
		GL( glDisable( GL_BLEND ) );

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

			// build the index array and render use it
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