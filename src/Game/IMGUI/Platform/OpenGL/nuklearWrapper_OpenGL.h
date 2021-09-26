#ifndef nuklearWrapper_OpenGL
#define nuklearWrapper_OpenGL

#include "Graphics/Platform/OpenGL/glPlatform.h"
#include "Graphics/Platform/OpenGL/glShaderManager.h"

typedef struct {
	GLuint vbo;
	GLuint vao;
	GLuint ebo;

	ShaderProgram prog;

	GLuint fontTx;
} NuklearWrapper_Platform;

#endif // inclusion guard