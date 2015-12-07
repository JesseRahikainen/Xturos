#include "triRendering.h"

//#include "../Others/glew.h"
//#include <SDL_opengl.h>

#include "../Math/matrix4.h"
#include "camera.h"
#include "shaderManager.h"
#include "glDebugging.h"

typedef struct {
	Vector3 pos;
	Color col;
	Vector2 uv;
} Vertex;

typedef struct {
	GLuint vertexIndices[3];
	float zPos;
	uint32_t camFlags;
	GLuint texture;

	ShaderType shaderType;
} Triangle;

/*
Ok, so what do we want to optimize for?
I'd think transferring memory.
So we have the vertices we transfer at the beginning of the rendering
Once that is done we generate index buffers to represent what each camera can see
*/
#define MAX_TRIS 2048
#define MAX_VERTS ( MAX_TRIS * 3 )

typedef struct {
	Triangle triangles[MAX_TRIS];
	Vertex vertices[MAX_VERTS];
	GLuint indices[MAX_VERTS];
	GLuint VAO;
	GLuint VBO;
	GLuint IBO;
	int lastTriIndex;
	int lastIndexBufferIndex;
} TriangleList;

TriangleList solidTriangles;
TriangleList transparentTriangles;

#define Z_ORDER_OFFSET ( 1.0f / (float)( 2 * ( MAX_TRIS + 1 ) ) )

static ShaderProgram shaderPrograms[NUM_SHADERS];

int triRenderer_LoadShaders( void )
{
	ShaderDefinition shaderDefs[3];
	ShaderProgramDefinition progDefs[NUM_SHADERS];

	shaders_Destroy( shaderPrograms, NUM_SHADERS );

	// Sprite shader
	shaderDefs[0].fileName = NULL;
	shaderDefs[0].type = GL_VERTEX_SHADER;
	shaderDefs[0].shaderText =	"#version 330\n"
								"uniform mat4 vpMatrix;\n"
								"layout(location = 0) in vec3 vVertex;\n"
								"layout(location = 1) in vec2 vTexCoord0;\n"
								"layout(location = 2) in vec4 vColor;\n"
								"out vec2 vTex;\n"
								"out vec4 vCol;\n"
								"void main( void )\n"
								"{\n"
								"	vTex = vTexCoord0;\n"
								"	vCol = vColor;\n"
								"	gl_Position = vpMatrix * vec4( vVertex, 1.0f );\n"
								"}\n";

	shaderDefs[1].fileName = NULL;
	shaderDefs[1].type = GL_FRAGMENT_SHADER;
	shaderDefs[1].shaderText =	"#version 330\n"
								"in vec2 vTex;\n"
								"in vec4 vCol;\n"
								"uniform sampler2D textureUnit0;\n"
								"out vec4 outCol;\n"
								"void main( void )\n"
								"{\n"
								"	outCol = texture2D(textureUnit0, vTex) * vCol;\n"
								"	if( outCol.w <= 0.0f ) {\n"
								"		discard;\n"
								"	}\n"
								"}\n";

	// for rendering fonts
	shaderDefs[2].fileName = NULL;
	shaderDefs[2].type = GL_FRAGMENT_SHADER;
	shaderDefs[2].shaderText =	"#version 330\n"
								"in vec2 vTex;\n"
								"in vec4 vCol;\n"
								"uniform sampler2D textureUnit0;\n"
								"out vec4 outCol;\n"
								"void main( void )\n"
								"{\n"
								"	outCol = vec4( vCol.r, vCol.g, vCol.b, texture2D(textureUnit0, vTex).a * vCol.a );\n"
								"	if( outCol.w <= 0.0f ) {\n"
								"		discard;\n"
								"	}\n"
								"}\n";

	progDefs[0].fragmentShader = 1;
	progDefs[0].vertexShader = 0;
	progDefs[0].geometryShader = -1;
	progDefs[0].uniformNames = "vpMatrix textureUnit0";

	progDefs[1].fragmentShader = 2;
	progDefs[1].vertexShader = 0;
	progDefs[1].geometryShader = -1;
	progDefs[1].uniformNames = "vpMatrix textureUnit0";

	if( shaders_Load( &( shaderDefs[0] ), sizeof( shaderDefs ) / sizeof( ShaderDefinition ),
		progDefs, shaderPrograms, NUM_SHADERS ) <= 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Error compiling image shaders.\n" );
		return -1;
	}

	return 0;
}

static int createTriListGLObjects( TriangleList* triList )
{
	GL( glGenVertexArrays( 1, &( triList->VAO ) ) );
	GL( glGenBuffers( 1, &( triList->VBO ) ) );
	GL( glGenBuffers( 1, &( triList->IBO ) ) );
	if( ( triList->VAO == 0 ) || ( triList->VBO == 0 ) || ( triList->IBO == 0 ) ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Unable to create one or more storage objects for triangle rendering." );
		return -1;
	}

	GL( glBindVertexArray( triList->VAO ) );

	GL( glBindBuffer( GL_ARRAY_BUFFER, triList->VBO ) );
	GL( glBufferData( GL_ARRAY_BUFFER, sizeof( triList->vertices ), NULL, GL_DYNAMIC_DRAW ) );

	GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, triList->IBO ) );
	GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( triList->indices ), triList->indices, GL_DYNAMIC_DRAW ) );

	GL( glEnableVertexAttribArray( 0 ) );
	GL( glEnableVertexAttribArray( 1 ) );
	GL( glEnableVertexAttribArray( 2 ) );

	GL( glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, pos ) ) );
	GL( glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, uv ) ) );
	GL( glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (const GLvoid*)offsetof( Vertex, col ) ) );

	GL( glBindVertexArray( 0 ) );

	GL( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );

	triList->lastIndexBufferIndex = -1;
	triList->lastTriIndex = -1;

	return 0;
}

/*
Initializes all the stuff needed for rendering the triangles.
 Returns a value < 0 if there's a problem.
*/
int triRenderer_Init( void )
{
	for( int i = 0; i < NUM_SHADERS; ++i ) {
		shaderPrograms[i].programID = 0;
	}

	if( triRenderer_LoadShaders( ) < 0 ) {
		return -1;
	}

	if( ( createTriListGLObjects( &solidTriangles ) < 0 ) ||
		( createTriListGLObjects( &transparentTriangles ) < 0 ) ) {
		return -1;
	}

	return 0;
}

int addTriangle( TriangleList* triList, Vector2 pos0, Vector2 pos1, Vector2 pos2, Vector2 uv0, Vector2 uv1, Vector2 uv2,
	ShaderType shader, GLuint texture, Color color, uint32_t camFlags, int8_t depth )
{
	if( triList->lastTriIndex >= ( MAX_TRIS - 1 ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_RENDER, "Triangle list full." );
		return -1;
	}

	float z = (float)depth + ( Z_ORDER_OFFSET * ( solidTriangles.lastTriIndex + transparentTriangles.lastTriIndex + 2 ) );

	int idx = triList->lastTriIndex + 1;
	triList->lastTriIndex = idx;
	triList->triangles[idx].camFlags = camFlags;
	triList->triangles[idx].texture = texture;
	triList->triangles[idx].zPos = z;
	triList->triangles[idx].shaderType = shader;
	int baseIdx = idx * 3;

	vec2ToVec3( &( pos0 ), z, &( triList->vertices[baseIdx].pos ) );
	triList->vertices[baseIdx].col = color;
	triList->vertices[baseIdx].uv = uv0;
	triList->triangles[idx].vertexIndices[0] = baseIdx;

	vec2ToVec3( &( pos1 ), z, &( triList->vertices[baseIdx+1].pos ) );
	triList->vertices[baseIdx+1].col = color;
	triList->vertices[baseIdx+1].uv = uv1;
	triList->triangles[idx].vertexIndices[1] = baseIdx + 1;

	vec2ToVec3( &( pos2 ), z, &( triList->vertices[baseIdx+2].pos ) );
	triList->vertices[baseIdx+2].col = color;
	triList->vertices[baseIdx+2].uv = uv2;
	triList->triangles[idx].vertexIndices[2] = baseIdx + 2;

	return 0;
}

/*
We'll assume the array has three vertices in it.
 Return a value < 0 if there's a problem.
*/
int triRenderer_AddVertices( Vector2* positions, Vector2* uvs, ShaderType shader, GLuint texture, Color color, uint32_t camFlags, int8_t depth, int transparent )
{
	return triRenderer_Add( positions[0], positions[1], positions[2], uvs[0], uvs[1], uvs[2],
		shader, texture, color, camFlags, depth, transparent );
}

int triRenderer_Add( Vector2 pos0, Vector2 pos1, Vector2 pos2, Vector2 uv0, Vector2 uv1, Vector2 uv2, ShaderType shader, GLuint texture,
	Color color, uint32_t camFlags, int8_t depth, int transparent )
{
	if( transparent ) {
		return addTriangle( &transparentTriangles, pos0, pos1, pos2, uv0, uv1, uv2, shader, texture, color, camFlags, depth );
	} else {
		return addTriangle( &solidTriangles, pos0, pos1, pos2, uv0, uv1, uv2, shader, texture, color, camFlags, depth );
	}
}

/*
Clears out all the triangles currently stored.
*/
void triRenderer_Clear( void )
{
	transparentTriangles.lastTriIndex = -1;
	solidTriangles.lastTriIndex = -1;
}

static int sortByRenderState( const void* p1, const void* p2 )
{
	Triangle* tri1 = (Triangle*)p1;
	Triangle* tri2 = (Triangle*)p2;

	int stDiff = ( (int)tri1->shaderType - (int)tri2->shaderType );
	if( stDiff != 0 ) {
		return stDiff;
	}

	if( tri1->texture < tri2->texture ) {
		return -1;
	} else if( tri1->texture > tri2->texture ) {
		return 1;
	}

	return 0;
}

static int sortByDepth( const void* p1, const void* p2 )
{
	Triangle* tri1 = (Triangle*)p1;
	Triangle* tri2 = (Triangle*)p2;
	return ( ( ( tri1->zPos ) - ( tri2->zPos ) ) > 0.0f ) ? 1 : -1;
}

static void generateVertexArray( TriangleList* triList )
{
	GL( glBindBuffer( GL_ARRAY_BUFFER, triList->VBO ) );
	GL( glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( Vertex ) * ( ( triList->lastTriIndex + 1 ) * 3 ), triList->vertices ) );
}

static void drawTriangles( uint32_t currCamera, TriangleList* triList )
{
	// create the index buffers to access the vertex buffer
	//  TODO: Test to see if having the index buffer or the vertex buffer in order is faster
	GLuint lastBoundTexture = 0;
	int triIdx = 0;
	ShaderType lastBoundShader = NUM_SHADERS;
	Matrix4 vpMat;
	uint32_t camFlags = 0;

	// we'll only be accessing the one vertex array
	GL( glBindVertexArray( triList->VAO ) );

	do {
		GLuint texture = triList->triangles[triIdx].texture;
		triList->lastIndexBufferIndex = -1;

		if( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].shaderType != lastBoundShader ) ) {
			// next shader, bind and set up
			lastBoundShader = triList->triangles[triIdx].shaderType;

			camFlags = cam_GetFlags( currCamera );
			cam_GetVPMatrix( currCamera, &vpMat );

			GL( glUseProgram( shaderPrograms[lastBoundShader].programID ) );
			GL( glUniformMatrix4fv( shaderPrograms[lastBoundShader].uniformLocs[0], 1, GL_FALSE, &( vpMat.m[0] ) ) );
			GL( glUniform1i( shaderPrograms[lastBoundShader].uniformLocs[1], 0 ) );
		}

		// gather the list of all the triangles to be drawn
		while( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].texture == texture ) &&
				( triList->triangles[triIdx].shaderType == lastBoundShader ) ) {
			if( ( triList->triangles[triIdx].camFlags & camFlags ) != 0 ) {
				triList->indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[0];
				triList->indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[1];
				triList->indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[2];
			}
			++triIdx;
		}

		// send the indices of the vertex array to draw, if there is any
		if( triList->lastIndexBufferIndex < 0 ) {
			continue;
		}
		GL( glBindTexture( GL_TEXTURE_2D, texture ) );
		GL( glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, sizeof( GLuint ) * ( triList->lastIndexBufferIndex + 1 ), triList->indices ) );
		GL( glDrawElements( GL_TRIANGLES, triList->lastIndexBufferIndex + 1, GL_UNSIGNED_INT, NULL ) );
	} while( triIdx <= triList->lastTriIndex );
}

/*
Draws out all the triangles.
*/
void triRenderer_Render( void )
{
	SDL_qsort( solidTriangles.triangles, solidTriangles.lastTriIndex + 1, sizeof( Triangle ), sortByRenderState );
	SDL_qsort( transparentTriangles.triangles, transparentTriangles.lastTriIndex + 1, sizeof( Triangle ), sortByDepth );

	// now that the triangles have been sorted create the vertex arrays
	generateVertexArray( &solidTriangles );
	generateVertexArray( &transparentTriangles );

	GL( glDisable( GL_CULL_FACE ) );
	GL( glEnable( GL_DEPTH_TEST ) );
	GL( glDepthMask( GL_TRUE ) );
	GL( glDepthFunc( GL_LESS ) );

	// render triangles
	// TODO: We're ignoring any issues with cameras and transparency, probably want to handle this better.
	for( int currCamera = cam_StartIteration( ); currCamera != -1; currCamera = cam_GetNextActiveCam( ) ) {
		GL( glClear( GL_DEPTH_BUFFER_BIT ) );
		
		GL( glDisable( GL_BLEND ) );
		drawTriangles( currCamera, &solidTriangles );

		GL( glEnable( GL_BLEND ) );
		GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
		drawTriangles( currCamera, &transparentTriangles );
	}

	GL( glBindVertexArray( 0 ) );
	GL( glUseProgram( 0 ) );
}