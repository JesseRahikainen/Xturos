#define _CRT_SECURE_NO_DEPRECATE
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include "../Graphics/glPlatform.h"

#include "shaderManager.h"

#include "glDebugging.h"
#include "../Utils/helpers.h"
#include "../System/memory.h"
#include "../System/platformLog.h"

// Loaded shader.
struct Shader
{
	GLenum type;
	GLuint id;
};

// Maximum length in characters a shader can be.
//TODO: Switch over to a dynamic string.
#define MAX_SHADER_SIZE 2048
static char shaderContents[MAX_SHADER_SIZE + 1];

static struct Shader* shaders;
static int numShaders;

static int isValidShader( int shaderIdx, struct Shader* shaderList )
{
	// < 0 is the flag for not using that shader, so it doesn't matter
	if( shaderIdx < 0 ) {
		return 1;
	}

	if( shaderIdx >= numShaders ) {
		llog( LOG_WARN, "Shader index not in valid range." );
		return 0;
	}

	GLboolean isShader = GL_FALSE;
	GLR( isShader, glIsShader( shaderList[shaderIdx].id ) );
	if( isShader == GL_FALSE ) {
		llog( LOG_WARN, "Shader index does not point to a valid shader." );
		return 0;
	}

	return 1;
}

static GLuint compileShader( const char* text, GLenum type )
{
	GLchar *shaderSourceStrs[1];
	GLint testVal;
	GLuint shaderID = 0;

	llog( LOG_VERBOSE, "Compiling shader" );

	if( text != NULL ) {
		GLR( shaderID, glCreateShader( type ) );
		llog( LOG_VERBOSE, "-- Created shader ID: %u", shaderID );

		if( shaderID != 0 ) {
			shaderSourceStrs[0] = (GLchar*)text;
			llog( LOG_VERBOSE, "-- Setting source and compiling" );
			GL( glShaderSource( shaderID, 1, (const GLchar**)shaderSourceStrs, NULL ) );
			GL( glCompileShader( shaderID ) );

			// now check for errors
			GL( glGetShaderiv( shaderID, GL_COMPILE_STATUS, &testVal ) );
			llog( LOG_VERBOSE, "-- GL_COMPILE_STATUS: %i", testVal );

			GLint logSize = 0;
			GL( glGetShaderiv( shaderID, GL_INFO_LOG_LENGTH, &logSize ) );
			if( logSize > 1 ) {
				GLchar* errorStr = (GLchar*)mem_Allocate( sizeof(GLchar) * logSize );
				if( errorStr != NULL ) {
					GL( glGetShaderInfoLog( shaderID, (GLsizei)logSize, NULL, errorStr ) );
					llog( LOG_ERROR, "Error compiling shader:\n - %s", errorStr );
					mem_Release( errorStr );
				} else {
					llog( LOG_ERROR, "Error allocating memory for error string" );
				}
			}

			if( testVal == GL_FALSE ) {
				llog( LOG_ERROR, "Error compiling shader" );

				GL( glDeleteShader( shaderID ) );
				shaderID = 0;
			}
		} else {
			llog( LOG_ERROR, "glCreateShader had a problem creating a shader." );
		}
	}

	llog( LOG_VERBOSE, "Returning shader: %u", shaderID );

	return shaderID;
}

static struct Shader loadShader( const ShaderDefinition* definition )
{
	FILE* inFile;
	size_t amountRead;
	struct Shader shader = { 0, 0 };
	
	llog( LOG_VERBOSE, "Loading shader file" );
	shader.type = definition->type;

	if( ( definition->shaderText != NULL ) && ( strlen( definition->shaderText ) > 0 ) ) {
		llog( LOG_VERBOSE, "-- Compiling shader from shader text" );
		shader.id = compileShader( definition->shaderText, shader.type );
		if( shader.id == 0 ) {
			llog( LOG_ERROR, "Error opening shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		}
	} else if( ( definition->fileName != NULL ) && ( strlen( definition->fileName ) > 0 ) ) {
		llog( LOG_VERBOSE, "-- Compiling shader from file" );
		inFile = fopen( definition->fileName, "r" );
		if( inFile == NULL ) {
			llog( LOG_ERROR, "Error opening shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			return shader;
		}

		memset( shaderContents, 0, sizeof(char) * ( MAX_SHADER_SIZE + 1 ) );
		amountRead = fread( shaderContents, sizeof(char), MAX_SHADER_SIZE, inFile );
		shaderContents[amountRead] = 0; // can't assume that fread won't use some of the buffer for something, so need to set this here
		llog( LOG_VERBOSE, "-- Read in %u bytes of text", amountRead );

		if( ferror( inFile ) || amountRead == 0 ) {
			llog( LOG_ERROR, "Nothing read in from shader file %s.", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		} else if ( amountRead == MAX_SHADER_SIZE ) {
			llog( LOG_ERROR, "Shader file %s is too big, increase max size or make shader smaller.", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		} else if ( amountRead < MAX_SHADER_SIZE ) {
			llog( LOG_VERBOSE, "COMPILING SHADER: %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			shader.id = compileShader( shaderContents, shader.type );
			if( shader.id == 0 ) {
				llog( LOG_ERROR, "Error compiling shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			}
		}

		fclose( inFile );
	} else {
		llog( LOG_ERROR, "Shader definition has no file or text." );
	}

	llog( LOG_VERBOSE, "-- Done compiling shader" );
	return shader;
}

// Returns the shader program id, as usual for these a value of 0 means it was unsuccessful
static GLuint createShaderProgram( const ShaderProgramDefinition* def, int logIdx )
{
	int anyMatch;
	GLuint programID;
	GLint testVal;
	llog( LOG_VERBOSE, "Creating shader program %i", logIdx );

	// make sure the shaders to use have been successfully compiled first
	if( !isValidShader( def->vertexShader, shaders ) ) {
		llog( LOG_ERROR, "Shader program at index %i is using invalid vertex shader %i", logIdx, def->vertexShader );
		return 0;
	}

	if( !isValidShader( def->fragmentShader, shaders ) ) {
		llog( LOG_ERROR, "Shader program at index %i is using invalid fragment shader %i", logIdx, def->fragmentShader );
		return 0;
	}

	if( !isValidShader( def->geometryShader, shaders ) ) {
		llog( LOG_ERROR, "Shader program at index %i is using invalid geometry shader %i", logIdx, def->geometryShader );
		return 0;
	}

	// Make sure the shaders are all at different indices, gives better output than just linking and failing.
	anyMatch = 0;
	if( ( def->fragmentShader == def->vertexShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		llog( LOG_ERROR, "Fragment and vertex shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( ( def->fragmentShader == def->geometryShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		llog( LOG_ERROR, "Fragment and geometry shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( ( def->vertexShader == def->geometryShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		llog( LOG_ERROR, "Vertex and geometry shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( anyMatch ) {
		return 0;
	}

	// create the program
	GLR( programID, glCreateProgram( ) );
	llog( LOG_VERBOSE, "-- creating gl program: %u", programID );
	if( programID == 0 ) {
		llog( LOG_WARN, "glCreateProgram returned invalid value for shader program defintion %i", logIdx );
		return 0;
	}

	// bind the shaders
	if( def->vertexShader >= 0 ) {
		llog( LOG_VERBOSE, "-- binding vertex shader %u", shaders[def->vertexShader].id );
		GL( glAttachShader( programID, shaders[def->vertexShader].id ) );
	}
	if( def->fragmentShader >= 0 ) {
		llog( LOG_VERBOSE, "-- binding fragment shader %u", shaders[def->fragmentShader].id );
		GL( glAttachShader( programID, shaders[def->fragmentShader].id ) );
	}
	if( def->geometryShader >= 0 ) {
		llog( LOG_VERBOSE, "-- binding geometry shader %u", shaders[def->geometryShader].id );
		GL( glAttachShader( programID, shaders[def->geometryShader].id ) );
	}

	// link the program, and make sure it was successful
	GL( glLinkProgram( programID ) );
	GL( glGetProgramiv( programID, GL_LINK_STATUS, &testVal ) );
	llog( LOG_VERBOSE, "-- linking program %s", ( testVal ? "TRUE" : "FALSE" ) );
	if( testVal == GL_FALSE ) {
		llog( LOG_WARN, "Shader program at index %i failed to link.", logIdx );
		GL( glDeleteProgram( programID ) );
		return 0;
	}

	return programID;
}

/* You create an array of ShaderDefinitions and ShaderProgramDefinitions that determine what is loaded.
You also create an empty array of GLuints to use as the final indices for the generated shader programs.
Returns the number of shader programs successfully created. */
size_t shaders_Load( const ShaderDefinition* shaderDefs, size_t numShaderDefs,
				const ShaderProgramDefinition* shaderProgDefs,
				ShaderProgram* shaderPrograms, size_t numShaderPrograms )
{
	llog( LOG_VERBOSE, "Loading shaders - definitions: %u  programs: %u", numShaderDefs, numShaderPrograms );
	size_t numSuccessful = 0;
	size_t i;

	// zero out, this also sets all the programs to invalid
	memset( shaderPrograms, 0, sizeof(GLuint) * numShaderPrograms );

	numShaders = numShaderDefs;
	shaders = (struct Shader*)mem_Allocate( sizeof(struct Shader) * numShaders );
	if( shaders == NULL ) {
		llog( LOG_ERROR, "Problem allocating memory for shaders." );
		return 0;
	}

	// first load all the individual shaders
	for( i = 0; i < numShaderDefs; ++i ) {
		shaders[i] = loadShader( shaderDefs + i );
	}

	// then set up the shader
	for( i = 0; i < numShaderPrograms; ++i ) {
		shaderPrograms[i].programID = createShaderProgram( shaderProgDefs+i, i );
		if( shaderPrograms[i].programID != 0 ) {
			++numSuccessful;
			llog( LOG_VERBOSE, "Successfully created shader program %i: %u", i, shaderPrograms[i].programID );
			// load the positions for the uniform and inputs
			size_t defIdx = 0;
			if( shaderPrograms[i].uniformLocs != NULL ) {
				llog( LOG_VERBOSE, "Finding shader program uniforms" );
				size_t namesLen = strlen( shaderProgDefs[i].uniformNames ) + 1;
				char namesCopy[256];
				if( namesLen < sizeof( namesCopy ) ) {
					memcpy( namesCopy, shaderProgDefs[i].uniformNames, namesLen );
					char* token = strtok( namesCopy, " " );
			
					while( token != NULL ) {
						GLR( shaderPrograms[i].uniformLocs[defIdx], glGetUniformLocation( shaderPrograms[i].programID, token ) );
						llog( LOG_VERBOSE, "-- Found uniform %u %s at %i", defIdx, token, shaderPrograms[i].uniformLocs[defIdx] );
						token = strtok( NULL, " " );
						++defIdx;
					}
				} else {
					llog( LOG_ERROR, "Error copying uniform name string for parsing shader program definition %d. String too long.", i );
				}

				llog( LOG_VERBOSE, "Found %u uniforms", defIdx );
			}

			while( defIdx < ARRAY_SIZE( shaderPrograms[i].uniformLocs ) ) {
				shaderPrograms[i].uniformLocs[defIdx] = -1;
				++defIdx;
			}
		}
	}

	// don't need the shaders any more
	llog( LOG_VERBOSE, "Deleting shaders" );
	for( i = 0; i < numShaderDefs; ++i ) {
		if( isValidShader( i, shaders ) ) {
			GL( glDeleteShader( shaders[i].id ) );
		}
	}
	llog( LOG_VERBOSE, "Freeing shaders" );
	mem_Release( shaders );

	return numSuccessful;
}

void shaders_Destroy( ShaderProgram* shaderPrograms, size_t numShaderPrograms )
{
	for( size_t i = 0; i < numShaderPrograms; ++i ) {
		GL( glDeleteProgram( shaderPrograms[i].programID ) );
		shaderPrograms[i].programID = 0;
		for( int u = 0; u < ARRAY_SIZE( shaderPrograms[i].uniformLocs ); ++u ) {
			shaderPrograms[i].uniformLocs[u] = -1;
		}
	}
}

//********** Diagnostic Functions
void shaders_ListUniforms( GLuint shaderID )
{
	GLint count;
	GLint maxSize;
	GL( glGetProgramiv( shaderID, GL_ACTIVE_UNIFORMS, &count) );
	llog( LOG_INFO, "Uniforms for program %i:\n", shaderID );
	if( count == 0 ) {
		llog( LOG_INFO, "    none\n" );
	}

	GL( glGetProgramiv( shaderID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxSize ) );
	char* uniformName = (char*)mem_Allocate( maxSize );
	if( uniformName == NULL ) {
		llog( LOG_WARN, "Error allocating memory for listing shader uniforms.\n" );
	}

	GLsizei length;
	GLint size;
	GLenum type;
	for( GLint i = 0; i < count; ++i ) {		
		GL( glGetActiveUniform( shaderID, i, maxSize, &length, &size, &type, uniformName ) );
		llog( LOG_INFO, "   %s\n", uniformName );
	}

	mem_Release( uniformName );
}