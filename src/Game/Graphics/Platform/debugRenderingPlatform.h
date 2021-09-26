#ifndef DEBUG_RENDERING_PLATFORM
#define DEBUG_RENDERING_PLATFORM

#include <stdbool.h>

#include "Graphics/debugRendering.h"

// this header acts as an interface for the platform specific graphic functions for debug rendering
bool debugRendererPlatform_Init( size_t bufferSize );
void debugRendererPlatform_Render( DebugVertex* debugBuffer, int lastDebugVert );

#endif