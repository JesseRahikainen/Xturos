#ifndef TRI_RENDERING_PLATFORM
#define TRI_RENDERING_PLATFORM

#include <stdbool.h>

#include "Graphics/triRendering_DataTypes.h"

// Interface for the platform specific code use by the tri renderer

bool triPlatform_LoadShaders( void );

bool triPlatform_InitTriList( TriangleList* triList, TriType listType );

void triPlatform_RenderStart( TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles );
void triPlatform_RenderForCamera( int cam, TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles );
void triPlatform_RenderEnd( void );

#endif
