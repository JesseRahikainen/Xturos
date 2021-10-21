#ifdef METAL_GFX
#ifndef GRAPHICS_DATA_TYPES
#define GRAPHICS_DATA_TYPES

// XCode doesn't like it when I include Metal.h here, so we'll just be using void* for right now.
typedef struct {
    void* mtlTexture;
} PlatformTexture;

typedef struct {
    uint32_t* indices;
    
    void* triBufferPool;
    void* idxBufferPool;
    
    void* activeTriBuffer;
} PlatformTriangleList;

#endif
#endif // METAL_GFX
