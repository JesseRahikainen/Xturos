#define _CRT_SECURE_NO_DEPRECATE
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include "../Others/glew.h"

#include "shaderManager.h"

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

static int isValidShader( int shaderIdx, struct Shader* shaders )
{
	// < 0 is the flag for not using that shader, so it doesn't matter
	if( shaderIdx < 0 ) {
		return 1;
	}

	if( shaderIdx >= numShaders ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_VIDEO, "Shader index not in valid range." );
		return 0;
	}

	if( glIsShader( shaders[shaderIdx].id ) == GL_FALSE ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_VIDEO, "Shader index does not point to a valid shader." );
		return 0;
	}

	return 1;
}

static GLuint compileShader( const char* text, GLenum type )
{
	GLchar *shaderSourceStrs[1];
	GLint testVal;
	GLuint shaderID = 0;

	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Compiling shader" );

	if( text != NULL ) {
		shaderID = glCreateShader( type );
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Created shader ID: %u", shaderID );

		if( shaderID != 0 ) {
			shaderSourceStrs[0] = (GLchar*)text;
			SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Setting source and compiling" );
			glShaderSource( shaderID, 1, (const GLchar**)shaderSourceStrs, NULL );
			glCompileShader( shaderID );

			// now check for errors
			glGetShaderiv( shaderID, GL_COMPILE_STATUS, &testVal );
			SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- GL_COMPILE_STATUS: %i", testVal );

			GLint logSize = 0;
			glGetShaderiv( shaderID, GL_INFO_LOG_LENGTH, &logSize );
			if( logSize > 1 ) {
				GLchar* errorStr = (GLchar*)malloc( sizeof(GLchar) * logSize );
				if( errorStr != NULL ) {
					glGetShaderInfoLog( shaderID, (GLsizei)logSize, NULL, errorStr );
					SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error compiling shader:\n - %s", errorStr );
					free( errorStr );
				} else {
					SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error allocating memory for error string" );
				}
			}

			if( testVal == GL_FALSE ) {
				SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error compiling shader" );

				glDeleteShader( shaderID );
				shaderID = 0;
			}
		} else {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "glCreateShader had a problem creating a shader." );
		}
	}

	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Returning shader: %u", shaderID );

	return shaderID;
}

static struct Shader loadShader( const ShaderDefinition* definition )
{
	FILE* inFile;
	size_t amountRead;
	struct Shader shader = { 0, 0 };
	
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Loading shader file" );
	shader.type = definition->type;

	if( ( definition->shaderText != NULL ) && ( strlen( definition->shaderText ) > 0 ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Compiling shader from shader text" );
		shader.id = compileShader( definition->shaderText, shader.type );
		if( shader.id == 0 ) {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error opening shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		}
	} else if( ( definition->fileName != NULL ) && ( strlen( definition->fileName ) > 0 ) ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Compiling shader from file" );
		inFile = fopen( definition->fileName, "r" );
		if( inFile == NULL ) {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error opening shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			return shader;
		}

		memset( shaderContents, 0, sizeof(char) * ( MAX_SHADER_SIZE + 1 ) );
		amountRead = fread( shaderContents, sizeof(char), MAX_SHADER_SIZE, inFile );
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Read in %u bytes of text", amountRead );

		if( amountRead == 0 ) {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Nothing read in from shader file %s.", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		} else if (amountRead == MAX_SHADER_SIZE) {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Shader file %s is too big, increase max size or make shader smaller.", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
		} else if (amountRead < MAX_SHADER_SIZE) {
			SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "COMPILING SHADER: %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			shader.id = compileShader( shaderContents, shader.type );
			if( shader.id == 0 ) {
				SDL_LogError(  SDL_LOG_CATEGORY_VIDEO, "Error compiling shader file %s", ( ( definition->fileName == NULL ) ? "<no-file-name>" : definition->fileName ) );
			}
		}

		fclose( inFile );
	} else {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Shader definition has no file or text." );
	}

	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Done compiling shader" );
	return shader;
}

// Returns the shader program id, as usual for these a value of 0 means it was unsuccessful
static GLuint createShaderProgram( const ShaderProgramDefinition* def, int logIdx )
{
	int anyMatch;
	GLuint programID;
	GLint testVal;
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Creating shader program %i", logIdx );

	// make sure the shaders to use have been successfully compiled first
	if( !isValidShader( def->vertexShader, shaders ) ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Shader program at index %i is using invalid vertex shader %i", logIdx, def->vertexShader );
		return 0;
	}

	if( !isValidShader( def->fragmentShader, shaders ) ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Shader program at index %i is using invalid fragment shader %i", logIdx, def->fragmentShader );
		return 0;
	}

	if( !isValidShader( def->geometryShader, shaders ) ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Shader program at index %i is using invalid geometry shader %i", logIdx, def->geometryShader );
		return 0;
	}

	// Make sure the shaders are all at different indices, gives better output than just linking and failing.
	anyMatch = 0;
	if( ( def->fragmentShader == def->vertexShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Fragment and vertex shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( ( def->fragmentShader == def->geometryShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Fragment and geometry shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( ( def->vertexShader == def->geometryShader ) && ( def->fragmentShader > 0 ) ) {
		anyMatch = 1;
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Vertex and geometry shader indices are the same for program at index %i, they should never be.", logIdx );
	}

	if( anyMatch ) {
		return 0;
	}

	// create the program
	programID = glCreateProgram( );
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- creating gl program: %u", programID );
	if( programID == 0 ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_VIDEO, "glCreateProgram returned invalid value for shader program defintion %i", logIdx );
		return 0;
	}

	// bind the shaders
	if( def->vertexShader >= 0 ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- binding vertex shader %u", shaders[def->vertexShader].id );
		glAttachShader( programID, shaders[def->vertexShader].id );
	}
	if( def->fragmentShader >= 0 ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- binding fragment shader %u", shaders[def->fragmentShader].id );
		glAttachShader( programID, shaders[def->fragmentShader].id );
	}
	if( def->geometryShader >= 0 ) {
		SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- binding geometry shader %u", shaders[def->geometryShader].id );
		glAttachShader( programID, shaders[def->geometryShader].id );
	}

	// link the program, and make sure it was successful
	glLinkProgram( programID );
	glGetProgramiv( programID, GL_LINK_STATUS, &testVal );
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- linking program %s", ( testVal ? "TRUE" : "FALSE" ) );
	if( testVal == GL_FALSE ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_VIDEO, "Shader program at index %i failed to link.", logIdx );
		glDeleteProgram( programID );
		return 0;
	}

	return programID;
}

/* You create an array of ShaderDefinitions and ShaderProgramDefinitions that determine what is loaded.
You also create an empty array of GLuints to use as the final indices for the generated shader programs.
Returns the number of shader programs successfully created. */
size_t loadShaders( const ShaderDefinition* shaderDefs, size_t numShaderDefs,
				const ShaderProgramDefinition* shaderProgDefs,
				ShaderProgram* shaderPrograms, size_t numShaderPrograms )
{
	SDL_LogVerbose(  SDL_LOG_CATEGORY_VIDEO, "Loading shaders - definitions: %u  programs: %u", numShaderDefs, numShaderPrograms );
	size_t numSuccessful = 0;
	size_t i;

	// zero out, this also sets all the programs to invalid
	memset( shaderPrograms, 0, sizeof(GLuint) * numShaderPrograms );

	numShaders = numShaderDefs;
	shaders = (struct Shader*)malloc( sizeof(struct Shader) * numShaders );
	if( shaders == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Problem allocating memory for shaders." );
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
			SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Successfully created shader program %i: %u", i, shaderPrograms[i].programID );
			// load the positions for the uniform and inputs
			size_t defIdx = 0;
			if( shaderPrograms[i].uniformLocs != NULL ) {
				SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Finding shader program uniforms" );
				size_t namesLen = strlen( shaderProgDefs[i].uniformNames ) + 1;
				char namesCopy[256];
				if( namesLen < sizeof( namesCopy ) ) {
					memcpy( namesCopy, shaderProgDefs[i].uniformNames, namesLen );
					char* token = strtok( namesCopy, " " );
			
					while( token != NULL ) {
						shaderPrograms[i].uniformLocs[defIdx] = glGetUniformLocation( shaderPrograms[i].programID, token );
						SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "-- Found uniform %u %s at %i", defIdx, token, shaderPrograms[i].uniformLocs[defIdx] );
						token = strtok( NULL, " " );
						++defIdx;
					}
				} else {
					SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error copying uniform name string for parsing shader program definition %d. String too long.", i );
				}

				SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Found %u uniforms", defIdx );
			}

			size_t uniformArraySize = sizeof( shaderPrograms[i].uniformLocs ) / sizeof( shaderPrograms[i].uniformLocs[0] );
			while( defIdx < uniformArraySize ) {
				shaderPrograms[i].uniformLocs[defIdx] = -1;
				++defIdx;
			}
		}
	}

	// don't need the shaders any more
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Deleting shaders" );
	for( i = 0; i < numShaderDefs; ++i ) {
		if( isValidShader( i, shaders ) ) {
			glDeleteShader( shaders[i].id );
		}
	}
	SDL_LogVerbose( SDL_LOG_CATEGORY_VIDEO, "Freeing shaders" );
	free( shaders );

	return numSuccessful;
}

//********** Diagnostic Functions
void listUniforms( GLuint shaderID )
{
	GLint count;
	GLint maxSize;
	glGetProgramiv( shaderID, GL_ACTIVE_UNIFORMS, &count);
	SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "Uniforms for program %i:\n", shaderID );
	if( count == 0 ) {
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "    none\n" );
	}

	glGetProgramiv( shaderID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxSize );
	char* uniformName = (char*)malloc( maxSize );
	if( uniformName == NULL ) {
		SDL_LogWarn( SDL_LOG_CATEGORY_VIDEO, "Error allocating memory for listing shader uniforms.\n" );
	}

	GLsizei length;
	GLint size;
	GLenum type;
	for( GLint i = 0; i < count; ++i ) {		
		glGetActiveUniform( shaderID, i, maxSize, &length, &size, &type, uniformName );
		SDL_LogInfo( SDL_LOG_CATEGORY_VIDEO, "   %s\n", uniformName );
	}

	free( uniformName );
}