#ifdef OPENGL_GFX
#ifndef GRAPHICS_DATA_TYPES
#define GRAPHICS_DATA_TYPES

#include "Graphics/Platform/OpenGL/glPlatform.h"

typedef struct {
	GLuint id;
} PlatformTexture;

typedef struct {
	GLuint* indices;

	GLuint VAO;
	GLuint VBO;
	GLuint IBO;
} PlatformTriangleList;

#endif
#endif // OPENGL_GFX
