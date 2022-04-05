#ifdef OPENGL_GFX

#include "Graphics/Platform/triRenderingPlatform.h"

#include "Graphics/Platform/OpenGL/glShaderManager.h"
#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/Platform/OpenGL/glDebugging.h"
#include "System/platformLog.h"
#include "Graphics/triRendering.h"
#include "Math/matrix4.h"
#include "System/memory.h"
#include "Graphics/camera.h"

static ShaderProgram shaderPrograms[NUM_SHADERS];

bool triPlatform_LoadShaders( void )
{
	for( int i = 0; i < NUM_SHADERS; ++i ) {
		shaderPrograms[i].programID = 0;
	}

	llog( LOG_INFO, "Loading triangle renderer shaders." );
	ShaderDefinition shaderDefs[7];
	ShaderProgramDefinition progDefs[NUM_SHADERS];

	llog( LOG_INFO, "  Destroying shaders." );
	shaders_Destroy( shaderPrograms, NUM_SHADERS );

	// Sprite shader
	shaderDefs[0].fileName = NULL;
	shaderDefs[0].type = GL_VERTEX_SHADER;
	shaderDefs[0].shaderText = DEFAULT_VERTEX_SHADER;

	shaderDefs[1].fileName = NULL;
	shaderDefs[1].type = GL_FRAGMENT_SHADER;
	shaderDefs[1].shaderText = DEFAULT_FRAG_SHADER;

	// for rendering fonts
	shaderDefs[2].fileName = NULL;
	shaderDefs[2].type = GL_FRAGMENT_SHADER;
	shaderDefs[2].shaderText = FONT_FRAG_SHADER;

	// simple sdf rendering
	shaderDefs[3].fileName = NULL;
	shaderDefs[3].type = GL_FRAGMENT_SHADER;
	shaderDefs[3].shaderText = SIMPLE_SDF_FRAG_SHADER;

	// image sdf rendering
	shaderDefs[4].fileName = NULL;
	shaderDefs[4].type = GL_FRAGMENT_SHADER;
	shaderDefs[4].shaderText = IMAGE_SDF_FRAG_SHADER;

	// outlined image sdf rendering
	shaderDefs[5].fileName = NULL;
	shaderDefs[5].type = GL_FRAGMENT_SHADER;
	shaderDefs[5].shaderText = OUTLINED_IMAGE_SDF_FRAG_SHADER;

	// alpha map sdf rendering
	shaderDefs[6].fileName = NULL;
	shaderDefs[6].type = GL_FRAGMENT_SHADER;
	shaderDefs[6].shaderText = ALPHA_MAPPED_SDF_FRAG_SHADE;


	progDefs[0].fragmentShader = 1;
	progDefs[0].vertexShader = 0;
	progDefs[0].geometryShader = -1;

	progDefs[1].fragmentShader = 2;
	progDefs[1].vertexShader = 0;
	progDefs[1].geometryShader = -1;

	progDefs[2].fragmentShader = 3;
	progDefs[2].vertexShader = 0;
	progDefs[2].geometryShader = -1;

	progDefs[3].fragmentShader = 4;
	progDefs[3].vertexShader = 0;
	progDefs[3].geometryShader = -1;

	progDefs[4].fragmentShader = 5;
	progDefs[4].vertexShader = 0;
	progDefs[4].geometryShader = -1;

	progDefs[5].fragmentShader = 6;
	progDefs[5].vertexShader = 0;
	progDefs[5].geometryShader = -1;

	llog( LOG_INFO, "  Loading shaders." );
	if( shaders_Load( &( shaderDefs[0] ), sizeof( shaderDefs ) / sizeof( ShaderDefinition ),
		progDefs, shaderPrograms, NUM_SHADERS ) <= 0 ) {
		llog( LOG_ERROR, "Error compiling image shaders.\n" );
		return false;
	}

	return true;
}

bool triPlatform_InitTriList( TriangleList* triList, TriType listType )
{
	if( ( triList->triangles = mem_Allocate( sizeof( Triangle ) * triList->triCount ) ) == NULL ) {
		llog( LOG_ERROR, "Unable to allocate triangles array." );
		return false;
	}

	if( ( triList->vertices = mem_Allocate( sizeof( Vertex ) * triList->vertCount ) ) == NULL ) {
		llog( LOG_ERROR, "Unable to allocate vertex array." );
		return false;
	}

	if( ( triList->platformTriList.indices = mem_Allocate( sizeof( GLuint ) * triList->vertCount ) ) == NULL ) {
		llog( LOG_ERROR, "Unable to allocate index array." );
		return false;
	}

	GL( glGenVertexArrays( 1, &( triList->platformTriList.VAO ) ) );
	GL( glGenBuffers( 1, &( triList->platformTriList.VBO ) ) );
	GL( glGenBuffers( 1, &( triList->platformTriList.IBO ) ) );
	if( ( triList->platformTriList.VAO == 0 ) || ( triList->platformTriList.VBO == 0 ) || ( triList->platformTriList.IBO == 0 ) ) {
		llog( LOG_ERROR, "Unable to create one or more storage objects for triangle rendering." );
		return false;
	}

	GL( glBindVertexArray( triList->platformTriList.VAO ) );

	GL( glBindBuffer( GL_ARRAY_BUFFER, triList->platformTriList.VBO ) );
	GL( glBufferData( GL_ARRAY_BUFFER, sizeof( triList->vertices[0] ) * triList->vertCount, NULL, GL_DYNAMIC_DRAW ) );

	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, triList->platformTriList.IBO ) );
	GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( triList->platformTriList.indices[0] ) * triList->vertCount, triList->platformTriList.indices, GL_DYNAMIC_DRAW ) );

	GL( glEnableVertexAttribArray( 0 ) );
	GL( glEnableVertexAttribArray( 1 ) );
	GL( glEnableVertexAttribArray( 2 ) );

	GL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, pos ) ) );
	GL( glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, uv ) ) );
	GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, col ) ) );

	GL( glBindVertexArray( 0 ) );

	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );

	return true;
}

static void fillTriDataArrays( TriangleList* triList )
{
	if( triList->lastTriIndex < 0 ) {
		// some OGL ES implementations doesn't like when you try to buffer data with a size of 0
		return;
	}
	GL( glBindBuffer( GL_ARRAY_BUFFER, triList->platformTriList.VBO ) );
	GL( glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( Vertex ) * ( ( triList->lastTriIndex + 1 ) * 3 ), triList->vertices ) );
}

// for when we're rendering to the stencil buffer
static void onStencilSwitch_Stencil( int stencilGroup )
{
	glStencilMask( 1 << stencilGroup );
}

// for when we should be reading from the stencil buffer
static void onStencilSwitch_Standard( int stencilGroup )
{
	if( ( stencilGroup >= 0 ) && ( stencilGroup <= 7 ) ) {
		// enable stencil masking
		glStencilFunc( GL_EQUAL, 0xff, ( 1 << stencilGroup ) );
	} else {
		// disable stencil masking
		glStencilFunc( GL_ALWAYS, 0xff, 0xff );
	}
}

static void drawTriangles( uint32_t currCamera, TriangleList* triList, void( *onStencilSwitch )( int ) )
{
	// create the index buffers to access the vertex buffer
	//  TODO: Test to see if having the index buffer or the vertex buffer in order is faster
	GLuint lastBoundTexture = 0;
	GLuint lastBoundExtraTexture = 0;
	int triIdx = 0;
	ShaderType lastBoundShader = NUM_SHADERS;
	Matrix4 vpMat;
	uint32_t camFlags = 0;
	int lastSetClippingArea = -1;

	// we'll only be accessing the one vertex array
	GL( glBindVertexArray( triList->platformTriList.VAO ) );

	do {
		GLuint texture = triList->triangles[triIdx].texture.id;
		GLuint extraTexture = triList->triangles[triIdx].extraTexture.id;
		float floatVal0 = triList->triangles[triIdx].floatVal0;
		triList->lastIndexBufferIndex = -1;

		if( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].shaderType != lastBoundShader ) ) {
			// next shader, bind and set up
			lastBoundShader = triList->triangles[triIdx].shaderType;

			camFlags = cam_GetFlags( currCamera );
			cam_GetVPMatrix( currCamera, &vpMat );

			GL( glUseProgram( shaderPrograms[lastBoundShader].programID ) );
			GL( glUniformMatrix4fv( shaderPrograms[lastBoundShader].uniformLocs[UNIFORM_TF_MAT], 1, GL_FALSE, &( vpMat.m[0] ) ) ); // set view projection matrix
			GL( glUniform1i( shaderPrograms[lastBoundShader].uniformLocs[UNIFORM_TEXTURE], 0 ) ); // use texture 0
			GL( glUniform1i( shaderPrograms[lastBoundShader].uniformLocs[UNIFORM_EXTRA_TEXTURE], 1 ) ); // use texture 1
		}

		if( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].stencilGroup != lastSetClippingArea ) ) {
			// next clipping area
			lastSetClippingArea = triList->triangles[triIdx].stencilGroup;
			onStencilSwitch( triList->triangles[triIdx].stencilGroup );
		}

		int triCount = 0;
		// gather the list of all the triangles to be drawn
		while( ( triIdx <= triList->lastTriIndex ) &&
			( triList->triangles[triIdx].texture.id == texture ) &&
			( triList->triangles[triIdx].extraTexture.id == extraTexture ) &&
			( triList->triangles[triIdx].shaderType == lastBoundShader ) &&
			( triList->triangles[triIdx].stencilGroup == lastSetClippingArea ) &&
			FLT_EQ( triList->triangles[triIdx].floatVal0, floatVal0 ) ) {
			if( ( triList->triangles[triIdx].camFlags & camFlags ) != 0 ) {
				triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[0];
				triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[1];
				triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[2];
			}
			++triIdx;
			++triCount;
		}

		// send the indices of the vertex array to draw, if there is any
		if( triList->lastIndexBufferIndex < 0 ) {
			continue;
		}
		GL( glUniform1f( shaderPrograms[lastBoundShader].uniformLocs[UNIFORM_FLOAT_0], floatVal0 ) );
		GL( glActiveTexture( GL_TEXTURE0 + 0 ) );
		GL( glBindTexture( GL_TEXTURE_2D, texture ) );
		GL( glActiveTexture( GL_TEXTURE0 + 1 ) );
		GL( glBindTexture( GL_TEXTURE_2D, extraTexture ) );
		GL( glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, sizeof( GLuint ) * ( triList->lastIndexBufferIndex + 1 ), triList->platformTriList.indices ) );
        
        
        //GLint sizeVert, sizeIdx;
        //GL( glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &sizeVert ) );
        //GL( glGetBufferParameteriv( GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &sizeIdx ) );
        //llog( LOG_DEBUG, "vs: %i  is: %i", (int)sizeof( triList->triangles[0] ), (int)sizeof( triList->platformTriList.indices[0] ) );
        //llog( LOG_DEBUG, "v: %i  i: %i", sizeVert, sizeIdx );
        //llog( LOG_DEBUG, "idxCnt: %i", triList->lastIndexBufferIndex + 1 );
        
        
		GL( glDrawElements( GL_TRIANGLES, triList->lastIndexBufferIndex + 1, GL_UNSIGNED_INT, 0 ) );
	} while( triIdx <= triList->lastTriIndex );
}

void triPlatform_RenderStart( TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles )
{
	// now that the triangles have been sorted create the vertex arrays
	fillTriDataArrays( solidTriangles );
	fillTriDataArrays( transparentTriangles );
	fillTriDataArrays( stencilTriangles );

	GL( glDisable( GL_CULL_FACE ) );
	GL( glEnable( GL_DEPTH_TEST ) );
	GL( glEnable( GL_STENCIL_TEST ) );
	GL( glClearStencil( 0x0 ) );

}

void triPlatform_RenderForCamera( int cam, TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles )
{
	GL( glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ) );

	GL( glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ) );
	GL( glDepthFunc( GL_ALWAYS ) );
	GL( glDepthMask( GL_FALSE ) );
	GL( glStencilFunc( GL_ALWAYS, 0xff, 0xff ) );
	GL( glStencilOp( GL_REPLACE, GL_REPLACE, GL_REPLACE ) );
	drawTriangles( cam, stencilTriangles, onStencilSwitch_Stencil );

	GL( glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ) );
	GL( glDepthFunc( GL_LESS ) );
	GL( glDepthMask( GL_TRUE ) );
	GL( glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP ) );
	GL( glStencilMask( 0xff ) );
	GL( glDisable( GL_BLEND ) );
	drawTriangles( cam, solidTriangles, onStencilSwitch_Standard );

    //llog( LOG_DEBUG, "draw transparent" );
	GL( glEnable( GL_BLEND ) );
	GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
	drawTriangles( cam, transparentTriangles, onStencilSwitch_Standard );
}

void triPlatform_RenderEnd( void )
{
	GL( glBindVertexArray( 0 ) );
	GL( glUseProgram( 0 ) );
}
#endif
