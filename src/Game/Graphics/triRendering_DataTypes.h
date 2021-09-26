#ifndef TRI_RENDERING_DATA_TYPES
#define TRI_RENDERING_DATA_TYPES

#include "Graphics/color.h"
#include "Math/vector2.h"
#include "Math/vector3.h"
#include "Graphics/triRendering.h"

#if defined( WIN32 ) || defined( __ANDROID__ ) || defined( __EMSCRIPTEN__ )
	#include "Graphics/Platform/OpenGL/graphicsDataTypes_OpenGL.h"
#else
	#warning "NO DATA TYPES FOR THIS GRAPHICS PLATFORM!"
#endif

typedef struct {
	Vector3 pos;
	Color col;
	Vector2 uv;
} Vertex;

typedef struct {
	unsigned int vertexIndices[3];
	float zPos;
	uint32_t camFlags;

	PlatformTexture texture;
	float floatVal0;

	ShaderType shaderType;

	int stencilGroup; // valid values are 0-7, anything else will cause it to be ignored
} Triangle;

typedef struct {
	int triCount;
	int vertCount;

	Triangle* triangles;
	Vertex* vertices;

	PlatformTriangleList platformTriList;

	int lastTriIndex;
	int lastIndexBufferIndex;
} TriangleList;

#endif