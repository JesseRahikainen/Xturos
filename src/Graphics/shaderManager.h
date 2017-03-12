#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include "glPlatform.h"

/*
TODO:
	Add ability to preprocess includes, either on the tool level or in game. Maybe other preprocessor functionality?
	Switch over the uniform names to an array so we don't have to tokenize strings. Maybe find a better way to do it.
*/

/*
We're assuming some things about the shaders.
Naming conventions: The equivilant attributes that shaders use will have the same name. So all vertex positions will
  have the same name, all vertex normals will have the same name, etc. We can then extend that as we need more
  attributes.
  Uniforms will act similarly.
  This will allow us to use a generic kind of binding.
  Note: For very special cases we should be able to ignore all this. So we should have a list of general ones that
        we will use alot, and then a way to define custom ones.
*/

typedef struct {
	const char* shaderText; // This is preferred, so we can embed shaders into code if needed.
	const char* fileName;   // If there is nothing in shaderText we then load this file and try to use that.
	GLenum type;
} ShaderDefinition;

typedef struct {
	/* these should match the indices of the definitions for the shaders
	they should be < 0 if they're not being used */
	int vertexShader;
	int fragmentShader;
	int geometryShader;

	// these will be space separated values listing up to 8 names, these will be loaded into
	//  corresponding spots in the Shader struct
	char* uniformNames;
} ShaderProgramDefinition;

typedef struct {
	GLuint programID;
	GLint uniformLocs[8];
} ShaderProgram;

/* You create an array of ShaderDefinitions and ShaderProgramDefinitions that determine what is loaded.
You also create an empty array of GLuints to use as the final indices for the generated shader programs.
Returns the number of shader programs successfully created. */
size_t shaders_Load( const ShaderDefinition* shaderDefs, size_t numShaderDefs,
			const ShaderProgramDefinition* shaderProgDefs, ShaderProgram* shaderPrograms, size_t numShaderPrograms );

/*
Destroys all the shader programs in the list, and invalidates their structures.
*/
void shaders_Destroy( ShaderProgram* shaderPrograms, size_t numShaderPrograms );

// These are used for diagnostic purposes
void shaders_ListUniforms( GLuint shaderID );

#endif