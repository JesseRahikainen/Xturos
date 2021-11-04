#ifdef METAL_GFX

// IMPORTANT NOTE: This has only been made to support iOS, MacOS may or may not work and hasn't been tested.
// TODO: Get this working with MacOS. May work, have done no testing.
// TODO: Get the stencil buffer working.
#include "Graphics/Platform/graphicsPlatform.h"
#include "Graphics/Platform/triRenderingPlatform.h"
#include "Graphics/Platform/debugRenderingPlatform.h"
#include "Graphics/triRendering_DataTypes.h"

#import <Metal/Metal.h>

#include "System/platformLog.h"
#include "Graphics/graphics.h"
#include <assert.h>
#include <SDL_syswm.h>
#include <SDL_video.h>

#include "System/memory.h"
#include "Graphics/camera.h"

#include "Math/matrix4.h"
#include <simd/simd.h>
#include "Utils/helpers.h"
#include "Utils/stretchyBuffer.h"

typedef struct {
    simd_float4x4 vpMat;
    float floatVal0;
} Uniforms;

// test stuff ************************
/*typedef struct {
    float pos[3];
    float clr[4];
    float uv[2];
} TestVertex;

static TestVertex vertData[] = {
    { { 0.0f, -0.75f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
    { { 0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
    { { -0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },

    { { 0.0f, 0.75f, -0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
    { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
    { { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
};

static uint32_t idxData[] = { 0, 1, 2, 3, 4, 5 };
//static uint32_t idxData[] = { 3, 4, 5, 0, 1, 2 };

id<MTLBuffer> vertBuffer;
id<MTLBuffer> idxBuffer;
id<MTLFunction> vertFunc;
id<MTLFunction> fragFunc;
id<MTLRenderPipelineState> testRenderPipeline;
id<MTLDepthStencilState> testDSState = nil;//*/

//**********************************************************************
// graphicsPlatform
@interface METAL_RendererData : NSObject
    @property (nonatomic, assign) SDL_MetalView view;
    @property (nonatomic, retain) CAMetalLayer* layer;
    @property (nonatomic, retain) id<MTLDevice> mtlDevice;
    @property (nonatomic, retain) id<MTLCommandQueue> cmdQueue;
    @property (nonatomic, retain) id<MTLLibrary> library;
    @property (nonatomic, retain) id<MTLCommandBuffer> cmdBuffer;
    @property (nonatomic, retain) id<MTLSamplerState> sampler;
    @property (nonatomic, retain) id<MTLTexture> renderTarget; // either one of the dynamic targets or the back render target
    @property (nonatomic, retain) id<MTLTexture> msaaTarget;
    @property (nonatomic, retain) id<MTLTexture> depthStencilTexture;
@end

@implementation METAL_RendererData
@end

//#define ENABLE_STENCIL
#define ENABLE_MSAA
#define NUM_SAMPLES 4

static METAL_RendererData* rendererData = NULL;

#define NUM_FRAME_DATA_BUFFERS 3
static uint currentFrameBufferIdx;
static dispatch_semaphore_t frameBufferSemaphore;

#define DIE_OFF_BUFFER_COUNT 10
typedef struct {
    id<MTLBuffer> buffer;
    bool inUse;
    int rendersSinceUsed;
} IndexBuffer;

typedef struct {
    IndexBuffer* sbPools;
} IndexBufferPool;

static IndexBufferPool indexPools[NUM_FRAME_DATA_BUFFERS];

static void initIndexBufferPools( void )
{
    for( int i = 0; i < NUM_FRAME_DATA_BUFFERS; ++i ) {
        indexPools[i].sbPools = NULL;
    }
}

static void clearCurrIndexBufferPool( void )
{
    int numActivePools = 0;
    for( size_t i = 0; i < sb_Count( indexPools[currentFrameBufferIdx].sbPools ); ++i ) {
        indexPools[currentFrameBufferIdx].sbPools[i].inUse = false;
        ++indexPools[currentFrameBufferIdx].sbPools[i].rendersSinceUsed;
        
        // see if this pool hasn't been used for a while, if it hasn't then delete it
        if( indexPools[currentFrameBufferIdx].sbPools[i].rendersSinceUsed > DIE_OFF_BUFFER_COUNT ) {
            indexPools[currentFrameBufferIdx].sbPools[i].buffer = nil;
            sb_Remove( indexPools[currentFrameBufferIdx].sbPools, i );
            --i;
            //llog( LOG_DEBUG, "Destroying old pool" );
        } else {
            ++numActivePools;
        }
    }
    
    //llog( LOG_DEBUG, "FrameBuffer %i pools: %i", currentFrameBufferIdx, numActivePools );
}

id<MTLBuffer> grabIndexBuffer( size_t size )
{
    // we'll want to look for a pool that fits the tighest, if we can't find one that
    //  fits then grab an existing one, destroy it, and make a bigger version of it
    //  if there are no open buffers, then create a new one that fits the desired size
    // we're assuming we'll never have more than SIZE_T_MAX pools at once
    // just for some testing, lets start out with just doing the tightest fit and creating
    //  a new one if we can't find it, ignoring the resizing (since Metal doesn't support
    //  it and we'd have to destroy it), if we stick with this we could count how long it's
    //  been since a buffer has been used, and if it's been too long destroy it
    size_t bestPool = SIZE_T_MAX;
    size_t bestDiff = UINT32_MAX;
    //size_t firstNonUsedFound = SIZE_T_MAX;
    for( size_t i = 0; i < sb_Count( indexPools[currentFrameBufferIdx].sbPools ); ++i ) {
        if( indexPools[currentFrameBufferIdx].sbPools[i].inUse ) {
            continue;
        }
        
        size_t currLen = (size_t)[indexPools[currentFrameBufferIdx].sbPools[i].buffer length];
        if( currLen >= size ) {
            size_t diff = currLen - size;
            if( diff < bestDiff ) {
                bestPool = i;
                bestDiff = diff;
            }
        }
    }
    
    if( bestPool == SIZE_T_MAX ) {
        // none found, create one
        IndexBuffer newIdxBuffer;
        newIdxBuffer.buffer = [rendererData.mtlDevice newBufferWithLength:size options:MTLResourceStorageModeShared];
        sb_Push( indexPools[currentFrameBufferIdx].sbPools, newIdxBuffer );
        bestPool = sb_Count( indexPools[currentFrameBufferIdx].sbPools ) - 1;
        //llog( LOG_DEBUG, "Creating new pool" );
    }
    
    indexPools[currentFrameBufferIdx].sbPools[bestPool].inUse = true;
    indexPools[currentFrameBufferIdx].sbPools[bestPool].rendersSinceUsed = 0;
    //llog( LOG_DEBUG, "Frame pool %u - buffer: %u", currentFrameBufferIdx, bestPool );
    return indexPools[currentFrameBufferIdx].sbPools[bestPool].buffer;
}


static void makeDepthStencilTexture( id<MTLTexture> renderTarget )
{ @autoreleasepool {
    MTLPixelFormat pf;
#ifdef ENABLE_STENCIL
    pf = MTLPixelFormatDepth32Float;
#else
    pf = MTLPixelFormatDepth32Float;
#endif
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:pf
        width:[renderTarget width]
        height:[renderTarget height]
        mipmapped:NO];
    
#ifdef ENABLE_MSAA
    descriptor.textureType = MTLTextureType2DMultisample;
    descriptor.sampleCount = NUM_SAMPLES;
#else
    descriptor.textureType = MTLTextureType2D;
#endif
    descriptor.usage = MTLTextureUsageRenderTarget;
    descriptor.storageMode = MTLStorageModePrivate;
    rendererData.depthStencilTexture = [rendererData.mtlDevice newTextureWithDescriptor:descriptor];
    [rendererData.depthStencilTexture setLabel:@"depth/stencil"];
} }

#ifdef ENABLE_MSAA
static void makeMSAATexture( id<MTLTexture> renderTarget )
{ @autoreleasepool {

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[renderTarget pixelFormat] width:[renderTarget width] height:[renderTarget height] mipmapped:NO];
    descriptor.textureType = MTLTextureType2DMultisample;
    descriptor.sampleCount = NUM_SAMPLES;
    descriptor.usage = MTLTextureUsageRenderTarget;
    descriptor.storageMode = MTLStorageModePrivate;
    rendererData.msaaTarget = [rendererData.mtlDevice newTextureWithDescriptor:descriptor];
    [rendererData.msaaTarget setLabel:@"MSAA Target"];
} }
#endif

bool gfxPlatform_Init( SDL_Window* window, int desiredRenderWidth, int desiredRenderHeight )
{ @autoreleasepool {
    // this is largely copied from SDL_render_metal.m
    SDL_SysWMinfo sysInfo;
    SDL_VERSION( &sysInfo.version );
    
    int finalWidth, finalHeight;
    gfx_calculateRenderSize( desiredRenderWidth, desiredRenderHeight, &finalWidth, &finalHeight );
    
    // check to see if metal is supported
    if( !SDL_GetWindowWMInfo( window, &sysInfo ) ) {
        llog( LOG_ERROR, "Unable to get system window information." );
        return false;
    }
    
    // TODO: Get this working with Macs, for right now this is only for iOS
    if( ( sysInfo.subsystem != SDL_SYSWM_COCOA ) && ( sysInfo.subsystem != SDL_SYSWM_UIKIT ) ) {
        llog( LOG_ERROR, "Metal render targets only Cocoa and UIKit video targets." );
        return false;
    }
    
    if( sysInfo.subsystem == SDL_SYSWM_COCOA ) llog( LOG_VERBOSE, "Using Cocoa." );
    if( sysInfo.subsystem == SDL_SYSWM_UIKIT ) llog( LOG_VERBOSE, "Using UIKit." );
    
    id<MTLDevice> mtlDevice;
    mtlDevice = MTLCreateSystemDefaultDevice();
    if( mtlDevice == NULL ) {
        llog( LOG_ERROR, "Unable to create Metal device" );
        return false;
    }
    
    SDL_MetalView view = NULL;
    view = SDL_Metal_CreateView( window );
    
    if( view == NULL ) {
        llog( LOG_ERROR, "Unable to create Metal view" );
        return false;
    }
    
    CAMetalLayer* layer = NULL;
    layer = (CAMetalLayer*)[(__bridge UIView*)view layer];
    
    layer.device = mtlDevice;
    //layer.framebufferOnly = false;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    rendererData = [[METAL_RendererData alloc] init];
    
    if( rendererData == nil ) {
        llog( LOG_ERROR, "Unable to create renderer data." );
        SDL_Metal_DestroyView( view );
        return false;
    }
    
    rendererData.view = view;
    rendererData.mtlDevice = mtlDevice;
    rendererData.layer = layer;
    rendererData.cmdQueue = [mtlDevice newCommandQueue];
    rendererData.library = [mtlDevice newDefaultLibrary];

    MTLSamplerDescriptor* samplerDesc = [MTLSamplerDescriptor new];
    samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    rendererData.sampler = [rendererData.mtlDevice newSamplerStateWithDescriptor:samplerDesc];
    
// testing stuff
    //testingSetup();
    
    currentFrameBufferIdx = 0;
    frameBufferSemaphore = dispatch_semaphore_create( NUM_FRAME_DATA_BUFFERS );
    
    initIndexBufferPools( );
    
    return true;
} }

int gfxPlatform_GetMaxSize( int desiredSize )
{
    return gfxPlatform_GetMaxTextureSize( );
}

int gfxPlatform_GetMaxTextureSize( void )
{
    // based on this:
    //  https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    //  looks like gpu family 1 and 2 support 8192 px, while all the others support 16384 px
    // default value
    int max = 16384;
    
    if( [rendererData.mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v1] ||
        [rendererData.mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] ) {
        max = 8192;
    }
    
    return max;
}

void gfxPlatform_DynamicSizeRender( float dt, float t, int renderWidth, int renderHeight,
    int windowRenderX0, int windowRenderY0, int windowRenderX1, int windowRenderY1, Color clearColor )
{
    // todo: figure out what to do with this, do we need this any more?
    gfxPlatform_StaticSizeRender( dt, t, clearColor );
}

void gfxPlatform_StaticSizeRender( float dt, float t, Color clearColor )
{ @autoreleasepool {
    
    // wait for an open frame to render with
    dispatch_semaphore_wait( frameBufferSemaphore, DISPATCH_TIME_FOREVER );
    
    // set which frame we're using
    currentFrameBufferIdx = ( currentFrameBufferIdx + 1 ) % NUM_FRAME_DATA_BUFFERS;
    
    clearCurrIndexBufferPool( );
    
    id<CAMetalDrawable> drawable = [rendererData.layer nextDrawable];
    if( !drawable ) {
        return;
    }
    
    rendererData.renderTarget = drawable.texture;
    
    if( ( [rendererData.depthStencilTexture width] != [rendererData.renderTarget width] ) ||
        ( [rendererData.depthStencilTexture height] != [rendererData.renderTarget height] ) ) {
        makeDepthStencilTexture( rendererData.renderTarget );
    }
    
#ifdef ENABLE_MSAA
    if( ( [rendererData.msaaTarget width] != [rendererData.renderTarget width] ) ||
        ( [rendererData.msaaTarget height] != [rendererData.renderTarget height] ) ) {
        makeMSAATexture( rendererData.renderTarget );
    }
#endif
        
    rendererData.cmdBuffer = [rendererData.cmdQueue commandBuffer];
    
    //testingRender( ); /*
    // initial clearing of render target
    [rendererData.cmdBuffer pushDebugGroup:@"Color buffer clear"];
    MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    passDesc.colorAttachments[0].texture = rendererData.renderTarget;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake( clearColor.r, clearColor.g, clearColor.b, clearColor.a );
    id<MTLRenderCommandEncoder> commandEncoder = [rendererData.cmdBuffer renderCommandEncoderWithDescriptor:passDesc];
    [commandEncoder endEncoding];
    [rendererData.cmdBuffer popDebugGroup];
    
#ifdef ENABLE_MSAA
    [rendererData.cmdBuffer pushDebugGroup:@"MSAA target clear"];
    passDesc.colorAttachments[0].texture = rendererData.msaaTarget;
    commandEncoder = [rendererData.cmdBuffer renderCommandEncoderWithDescriptor:passDesc];
    [commandEncoder endEncoding];
    [rendererData.cmdBuffer popDebugGroup];
#endif
    
    gfx_MakeRenderCalls( dt, t );//*/
    
    // finish rendering
    [rendererData.cmdBuffer presentDrawable:drawable];
    
    __weak dispatch_semaphore_t semaphore = frameBufferSemaphore;
    [rendererData.cmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> cmdBuffer) {
        dispatch_semaphore_signal( semaphore );
    }];
    
    [rendererData.cmdBuffer commit];
} }

void gfxPlatform_CleanUp( void )
{
    
}

void gfxPlatform_ShutDown( void )
{
    if( rendererData != NULL ) {
        [rendererData release];
    }
}

void gfxPlatform_RenderResize( int newDesiredRenderWidth, int newDesiredRenderHeight )
{
    assert( 0 && "Not implemented yet!" );
}

bool gfxPlatform_CreateTextureFromLoadedImage( TextureFormat texFormat, LoadedImage* image, Texture* outTexture )
{ @autoreleasepool {
    
    outTexture->width = image->width;
    outTexture->height = image->height;
    outTexture->flags = 0;
    // check to see if there's any transluceny in the image
    for( int i = 0; ( i < ( image->width * image->height ) ) && !( outTexture->flags & TF_IS_TRANSPARENT ); ++i ) {
        if( ( image->data[i] > 0x00 ) && ( image->data[i] < 0xFF ) ) {
            outTexture->flags |= TF_IS_TRANSPARENT;
        }
    }
    
    // now create the texture in metal
    MTLPixelFormat format;
    NSUInteger bytesPerPixel = 0;
    switch( texFormat ) {
        case TF_RED:
            bytesPerPixel = 1;
            format = MTLPixelFormatR8Unorm;
            break;
            break;
        case TF_ALPHA:
            bytesPerPixel = 1;
            format = MTLPixelFormatA8Unorm;
            break;
        case TF_RGBA:
            bytesPerPixel = 4;
            format = MTLPixelFormatRGBA8Unorm;
            break;
        default:
            llog( LOG_ERROR, "No valid texture format." );
            return false;
    }
    
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:(NSUInteger)image->width height:(NSUInteger)image->height mipmapped:NO];
    
    id<MTLTexture> mtlTexture = [rendererData.mtlDevice newTextureWithDescriptor:texDesc];
    
    if( mtlTexture == nil ) {
        llog( LOG_ERROR, "Unable to create texture." );
        return false;
    }
    
    MTLRegion region = MTLRegionMake2D( 0, 0, (NSUInteger)image->width, (NSUInteger)image->height );
    NSUInteger bytesPerRow = bytesPerPixel * image->width;
    [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:image->data bytesPerRow:bytesPerRow];
    
    outTexture->texture.mtlTexture = (void*)CFBridgingRetain(mtlTexture);
    
    return true;
} }

bool gfxPlatform_CreateTextureFromSurface( SDL_Surface* surface, Texture* outTexture )
{
    outTexture->width = surface->w;
    outTexture->height = surface->h;
    outTexture->flags = 0;
    if( gfxUtil_SurfaceIsTranslucent( surface ) ) {
        outTexture->flags = TF_IS_TRANSPARENT;
    }
    
    // now create the texture in metal
    MTLPixelFormat format;
    NSUInteger bytesPerPixel = 0;
    if( surface->format->BytesPerPixel == 4 ) {
        format = MTLPixelFormatRGBA8Unorm;
        bytesPerPixel = 4;
    } else {
        llog( LOG_ERROR, "Unable to handle format." );
        return false;
    }
    
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:(NSUInteger)surface->w height:(NSUInteger)surface->h mipmapped:NO];
    
    id<MTLTexture> mtlTexture = [rendererData.mtlDevice newTextureWithDescriptor:texDesc];
    
    if( mtlTexture == nil ) {
        llog( LOG_ERROR, "Unable to create texture." );
        return false;
    }
    
    MTLRegion region = MTLRegionMake2D( 0, 0, (NSUInteger)surface->w, (NSUInteger)surface->h );
    NSUInteger bytesPerRow = bytesPerPixel * surface->w;
    [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:surface->pixels bytesPerRow:bytesPerRow];
    
    outTexture->texture.mtlTexture = (void*)CFBridgingRetain(mtlTexture);
    
    return true;
}

void gfxPlatform_UnloadTexture( Texture* texture )
{
    // TODO: Test this!
    CFRelease( texture->texture.mtlTexture );
    texture->texture.mtlTexture = NULL;
    texture->flags = 0;
}

int gfxPlatform_ComparePlatformTextures( PlatformTexture rhs, PlatformTexture lhs )
{
    // TODO: Test this!
    return rhs.mtlTexture == lhs.mtlTexture;
    return 0;
}

void gfxPlatform_DeletePlatformTexture( PlatformTexture texture )
{
    // TODO: Test this!
    CFRelease( texture.mtlTexture );
}


void gfxPlatform_Swap( SDL_Window* window )
{
    // nothing to do here
}

uint8_t* gfxPlatform_GetScreenShotPixels( int width, int height )
{
    SDL_assert( "Screen shot not implemented for Metal yet." );
    return NULL;
}

PlatformTexture gfxPlatform_GetDefaultPlatformTexture( void )
{
    PlatformTexture pt;
    pt.mtlTexture = 0;
    return pt;
}

//**********************************************************************
// triRendering

typedef enum {
    PT_SOLID,
    PT_TRANSPARENT,
    PT_STENCIL,
    NUM_PIPELINE_TYPES
} PipelineType;

id<MTLRenderPipelineState> triPipelines[NUM_SHADERS][NUM_PIPELINE_TYPES];

typedef struct {
    id<MTLBuffer> solidTriBuffer;
    id<MTLBuffer> transparentTriBuffer;
    id<MTLBuffer> stencilTriBuffer;
} METAL_FrameTriangleBuffers;

static METAL_FrameTriangleBuffers frameTriangleBuffers[NUM_FRAME_DATA_BUFFERS];

@interface METAL_TriRendererData : NSObject
    @property (nonatomic, retain) NSMutableDictionary* writeStencilPassDSStates;
    @property (nonatomic, retain) NSMutableDictionary* readStencilPassDSStates;
@end

@implementation METAL_TriRendererData
@end

static METAL_TriRendererData* triRendererData = NULL;

id<MTLBuffer> triDynamicDataBuffers[NUM_FRAME_DATA_BUFFERS];
id<MTLDepthStencilState> nonStencilPassIgnoreDSState = nil;

// TODO: Get rid of these once we're using the pools
id<MTLBuffer> currSolidTriVertBuffer = nil;
id<MTLBuffer> currTransparentTriVertBuffer = nil;
id<MTLBuffer> currStencilTriVertBuffer = nil;

// HACK: because we can't get the stencil buffer working, but we only have to
//  worry about rectangular areas
static MTLScissorRect scissorRects[8];
static bool scissorsValid[8];

static void createTriPipeline( id<MTLFunction> vertFunc, id<MTLFunction> fragFunc, MTLVertexDescriptor* vertDesc, ShaderType st )
{ @autoreleasepool {
    
    MTLRenderPipelineDescriptor* transparentPLDesc = [MTLRenderPipelineDescriptor new];
    transparentPLDesc.vertexFunction = vertFunc;
    transparentPLDesc.fragmentFunction = fragFunc;
    transparentPLDesc.vertexDescriptor = vertDesc;
    transparentPLDesc.colorAttachments[0].pixelFormat = rendererData.layer.pixelFormat;
#ifdef STENCIL_ENABLED
    transparentPLDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    transparentPLDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
#else
    transparentPLDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
#endif
    transparentPLDesc.colorAttachments[0].blendingEnabled = true;
    transparentPLDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    transparentPLDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    transparentPLDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    transparentPLDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    transparentPLDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    transparentPLDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
#ifdef ENABLE_MSAA
    transparentPLDesc.sampleCount = NUM_SAMPLES;
#endif
    
    NSError* err = nil;
    triPipelines[st][PT_TRANSPARENT] = [rendererData.mtlDevice newRenderPipelineStateWithDescriptor:transparentPLDesc error:&err];
    
    SDL_assert( err == nil );
    
    //
    
    MTLRenderPipelineDescriptor* solidPLDesc = [MTLRenderPipelineDescriptor new];
    solidPLDesc.vertexFunction = vertFunc;
    solidPLDesc.fragmentFunction = fragFunc;
    solidPLDesc.vertexDescriptor = vertDesc;
    solidPLDesc.colorAttachments[0].pixelFormat = rendererData.layer.pixelFormat;
#ifdef STENCIL_ENABLED
    solidPLDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    solidPLDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
#else
    solidPLDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
#endif
    solidPLDesc.colorAttachments[0].blendingEnabled = false;
#ifdef ENABLE_MSAA
    solidPLDesc.sampleCount = NUM_SAMPLES;
#endif
    
    err = nil;
    triPipelines[st][PT_SOLID] = [rendererData.mtlDevice newRenderPipelineStateWithDescriptor:solidPLDesc error:&err];
    
    SDL_assert( err == nil );
    
    //

#ifdef STENCIL_ENABLED
    MTLRenderPipelineDescriptor* stencilPLDesc = [MTLRenderPipelineDescriptor new];
    stencilPLDesc.vertexFunction = vertFunc;
    stencilPLDesc.fragmentFunction = fragFunc;
    stencilPLDesc.vertexDescriptor = vertDesc;
    stencilPLDesc.colorAttachments[0].pixelFormat = MTLPixelFormatInvalid;
    stencilPLDesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
    stencilPLDesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
#ifdef ENABLE_MSAA
    stencilPLDesc.sampleCount = NUM_SAMPLES;
#endif
    
    err = nil;
    triPipelines[st][PT_STENCIL] = [rendererData.mtlDevice newRenderPipelineStateWithDescriptor:stencilPLDesc error:&err];
    
    SDL_assert( err == nil );
#endif
} }

bool triPlatform_LoadShaders( void )
{ @autoreleasepool {
    // create the vertex descriptor
    MTLVertexDescriptor* vertDesc = [MTLVertexDescriptor vertexDescriptor];
    vertDesc.attributes[0].format = MTLVertexFormatFloat3; // pos
    vertDesc.attributes[0].bufferIndex = 0;
    vertDesc.attributes[0].offset = 0;
    
    vertDesc.attributes[1].format = MTLVertexFormatFloat4; // color
    vertDesc.attributes[1].bufferIndex = 0;
    vertDesc.attributes[1].offset = sizeof(float) * 3;
    
    vertDesc.attributes[2].format = MTLVertexFormatFloat2; // uv
    vertDesc.attributes[2].bufferIndex = 0;
    vertDesc.attributes[2].offset = sizeof(float) * 7;
    
    vertDesc.layouts[0].stride = sizeof(Vertex);
    vertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    // first get all the functions from the library
    id<MTLFunction> vertFunc = [rendererData.library newFunctionWithName:@"vertex_default"];
    id<MTLFunction> defaultFragFunc = [rendererData.library newFunctionWithName:@"fragment_default"];
    //id<MTLFunction> defaultFragFunc = [rendererData.library newFunctionWithName:@"fragment_colorOnly"];
    id<MTLFunction> fontFragFunc = [rendererData.library newFunctionWithName:@"fragment_font"];
    id<MTLFunction> simpleSDFFragFunc = [rendererData.library newFunctionWithName:@"fragment_simpleSDF"];
    id<MTLFunction> imageSDFFragFunc = [rendererData.library newFunctionWithName:@"fragment_imageSDF"];
    id<MTLFunction> outlinedImageSDFFragFunc = [rendererData.library newFunctionWithName:@"fragment_outlinedImageSDF"];
    id<MTLFunction> imageSDFAlphaMapFragFunc = [rendererData.library newFunctionWithName:@"fragment_SDFAlphaMap"];
    
    // create all the necessary pipelines
    createTriPipeline( vertFunc, defaultFragFunc, vertDesc, ST_DEFAULT );
    createTriPipeline( vertFunc, fontFragFunc, vertDesc, ST_ALPHA_ONLY );
    createTriPipeline( vertFunc, simpleSDFFragFunc, vertDesc, ST_SIMPLE_SDF );
    createTriPipeline( vertFunc, imageSDFFragFunc, vertDesc, ST_IMAGE_SDF );
    createTriPipeline( vertFunc, outlinedImageSDFFragFunc, vertDesc, ST_OUTLINED_IMAGE_SDF );
    createTriPipeline( vertFunc, imageSDFAlphaMapFragFunc, vertDesc, ST_ALPHA_MAPPED_SDF );
    
    // used when ignoring the stencil buffer, will have to dynamically create the one to use
    //  when using it (or create them as needed, otherwise there's a lot)
    MTLDepthStencilDescriptor* nonStencilPassDSDesc = [MTLDepthStencilDescriptor new];
    nonStencilPassDSDesc.depthCompareFunction = MTLCompareFunctionLess;
    nonStencilPassDSDesc.depthWriteEnabled = true;
    nonStencilPassDSDesc.backFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
    nonStencilPassDSDesc.backFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.backFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.backFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.backFaceStencil.readMask = 0xFF;
    nonStencilPassDSDesc.backFaceStencil.writeMask = 0xFF;
    nonStencilPassDSDesc.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
    nonStencilPassDSDesc.frontFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.frontFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
    nonStencilPassDSDesc.frontFaceStencil.readMask = 0xFF;
    nonStencilPassDSDesc.frontFaceStencil.writeMask = 0xFF;
    
    nonStencilPassIgnoreDSState = [rendererData.mtlDevice newDepthStencilStateWithDescriptor:nonStencilPassDSDesc];
    
    triRendererData = [[METAL_TriRendererData alloc] init];
    triRendererData.writeStencilPassDSStates = [NSMutableDictionary dictionary];
    triRendererData.readStencilPassDSStates = [NSMutableDictionary dictionary];
    
    return true;
} }

bool triPlatform_InitTriList( TriangleList* triList, TriType listType )
{
    if( ( triList->triangles = mem_Allocate( sizeof( Triangle ) * triList->triCount ) ) == NULL ) {
        llog( LOG_ERROR, "Unable to allocate triangles array." );
        return false;
    }
    
    if( ( triList->vertices = mem_Allocate( sizeof( Vertex ) * triList->vertCount ) ) == NULL ) {
        llog( LOG_ERROR, "Unable to allocate vertex array." );
        return false;
    }
    
    if( ( triList->platformTriList.indices = mem_Allocate( sizeof( uint32_t ) * triList->vertCount ) ) == NULL ) {
        llog( LOG_ERROR, "Unable to allocate index array." );
        return false;
    }

    // create the frame data buffers for each tri list
    for( int i = 0; i < NUM_FRAME_DATA_BUFFERS; ++i ) {
        id<MTLBuffer> dataBuffer = [rendererData.mtlDevice
                                    newBufferWithLength: sizeof( Vertex ) * triList->vertCount
                                    options:MTLResourceStorageModeShared];
        switch( listType ) {
        case TT_SOLID:
            frameTriangleBuffers[i].solidTriBuffer = dataBuffer;
            break;
        case TT_TRANSPARENT:
            frameTriangleBuffers[i].transparentTriBuffer = dataBuffer;
            break;
        case TT_STENCIL:
            frameTriangleBuffers[i].stencilTriBuffer = dataBuffer;
            break;
        }
    }
    
    return true;
}

id<MTLBuffer> setupTriListBufferData( TriangleList* triList )
{
    assert( 0 && "DON'T USE THIS" );
    // grab the buffer to use
    // TODO: Make this work with a pool of buffers instead of creating them each frame.
    id<MTLBuffer> triVertBuffer = [rendererData.mtlDevice
                               newBufferWithBytes: triList->vertices
                               length: sizeof(Vertex) * ( triList->vertCount )
                               options: MTLResourceOptionCPUCacheModeDefault];
    return triVertBuffer;
}

void triPlatform_RenderStart( TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles )
{
    currSolidTriVertBuffer = frameTriangleBuffers[currentFrameBufferIdx].solidTriBuffer;
    currTransparentTriVertBuffer = frameTriangleBuffers[currentFrameBufferIdx].transparentTriBuffer;
    currStencilTriVertBuffer = frameTriangleBuffers[currentFrameBufferIdx].stencilTriBuffer;
    
    void* solidBufferContents = [currSolidTriVertBuffer contents];
    memcpy( solidBufferContents, solidTriangles->vertices, sizeof(Vertex) * solidTriangles->vertCount );
    
    void* transparentBufferContents = [currTransparentTriVertBuffer contents];
    memcpy( transparentBufferContents, transparentTriangles->vertices, sizeof(Vertex) * transparentTriangles->vertCount );
    
    void* stencilBufferContents = [currStencilTriVertBuffer contents];
    memcpy( stencilBufferContents, stencilTriangles->vertices, sizeof(Vertex) * stencilTriangles->vertCount );
}

static id<MTLDepthStencilState> onStencilSwitch_Stencil( int stencilGroup )
{ @autoreleasepool {
    
    // check for existing state, if it doesn't exist then create it
    NSNumber* key = [NSNumber numberWithInt:stencilGroup];
    if( [triRendererData.writeStencilPassDSStates objectForKey:key] == nil ) {
        MTLDepthStencilDescriptor* passDSDesc = [MTLDepthStencilDescriptor new];
        passDSDesc.depthCompareFunction = MTLCompareFunctionAlways;
        passDSDesc.depthWriteEnabled = false;
        passDSDesc.backFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
        passDSDesc.backFaceStencil.stencilFailureOperation = MTLStencilOperationReplace;
        passDSDesc.backFaceStencil.depthFailureOperation = MTLStencilOperationReplace;
        passDSDesc.backFaceStencil.depthStencilPassOperation = MTLStencilOperationReplace;
        passDSDesc.backFaceStencil.readMask = 0xFF;
        passDSDesc.backFaceStencil.writeMask = ( 1 << stencilGroup );
        passDSDesc.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
        passDSDesc.frontFaceStencil.stencilFailureOperation = MTLStencilOperationReplace;
        passDSDesc.frontFaceStencil.depthFailureOperation = MTLStencilOperationReplace;
        passDSDesc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationReplace;
        passDSDesc.frontFaceStencil.readMask = 0xFF;
        passDSDesc.frontFaceStencil.writeMask = ( 1 << stencilGroup );
        
        id<MTLDepthStencilState> state = [rendererData.mtlDevice newDepthStencilStateWithDescriptor:passDSDesc];
        
        triRendererData.writeStencilPassDSStates[key] = state;
    }
    
    return triRendererData.writeStencilPassDSStates[key];
} }

static id<MTLDepthStencilState> onStencilSwitch_Standard( int stencilGroup )
{ @autoreleasepool {
    if( ( stencilGroup >= 0 ) && ( stencilGroup <= 7 ) ) {
        
        // see if there is an existing state we can use for this
        NSNumber* key = [NSNumber numberWithInt:stencilGroup];
        if( triRendererData.readStencilPassDSStates[key] == nil ) {
            MTLDepthStencilDescriptor* nonStencilPassDSDesc = [MTLDepthStencilDescriptor new];
            nonStencilPassDSDesc.depthCompareFunction = MTLCompareFunctionLess;
            nonStencilPassDSDesc.depthWriteEnabled = true;
            nonStencilPassDSDesc.backFaceStencil.stencilCompareFunction = MTLCompareFunctionEqual;
            nonStencilPassDSDesc.backFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.backFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.backFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.backFaceStencil.readMask = ( 1 << stencilGroup );
            nonStencilPassDSDesc.backFaceStencil.writeMask = 0x00;
            nonStencilPassDSDesc.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionEqual;
            nonStencilPassDSDesc.frontFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.frontFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
            nonStencilPassDSDesc.frontFaceStencil.readMask = ( 1 << stencilGroup );
            nonStencilPassDSDesc.frontFaceStencil.writeMask = 0x00;
            
            triRendererData.readStencilPassDSStates[key] = [rendererData.mtlDevice newDepthStencilStateWithDescriptor:nonStencilPassDSDesc];
        }
        
        return triRendererData.readStencilPassDSStates[key];
    } else {
        return nonStencilPassIgnoreDSState;
    }
} }

// HACK: Since we're only using the stencil for rectangular areas this should work
//  for what we need it right now, but we'll want to figure out the stencil buffer
//  later
#include <float.h>
static void scissorsStencilHack( int cam, TriangleList* triList, unsigned long renderWidth, unsigned long renderHeight )
{
    Vector2 mins[8];
    Vector2 maxes[8];
    
    Matrix4 vpMat;
    cam_GetVPMatrix( cam, &vpMat );
    
    // reset all the clipping rectangles
    for( int i = 0; i < 8; ++i ) {
        mins[i].x = mins[i].y = FLT_MAX;
        maxes[i].x = maxes[i].y = -FLT_MAX;
        scissorsValid[i] = false;
    }
    
    // go through each triangle generating mins and maxes
    for( int i = 0; i < triList->triCount; ++i ) {
        int group = triList->triangles[i].stencilGroup;
        if( group < 0 ) continue;
        
        for( int a = 0; a < 3; ++a ) {
            Vector3 p = triList->vertices[triList->triangles[i].vertexIndices[a]].pos;
            
            // this transform maps it to where the geometry would be drawn in the
            //  space used by the scissor rectangle
            mat4_TransformVec3Pos_InPlace( &vpMat, &p );
            p.x = ( ( p.x + 1.0f ) / 2.0f ) * renderWidth;
            p.y = ( ( -p.y + 1.0f ) / 2.0f ) * renderHeight;
            
            if( p.x < mins[group].x ) mins[group].x = p.x;
            if( p.y < mins[group].y ) mins[group].y = p.y;
            if( p.x > maxes[group].x ) maxes[group].x = p.x;
            if( p.y > maxes[group].y ) maxes[group].y = p.y;
            
            //llog( LOG_DEBUG, "pt: %f, %f", p.x, p.y );
            
            scissorsValid[i] = true;
        }
    }
    
    // convert mins and maxes to rect (top-left corner and size)
    for( int i = 0; i < 8; ++i ) {
        if( !scissorsValid[i] ) continue;
        
        // a scissor rect is only valid if it would collide with the
        //  render area (0, 0, renderWidth, renderHeight)
        // also make sure it fits on the screen
        unsigned long minX = (unsigned long)MIN( renderWidth, MAX( 0.0f, mins[i].x ) );
        unsigned long minY = (unsigned long)MIN( renderHeight, MAX( 0.0f, mins[i].y ) );
        unsigned long maxX = (unsigned long)MIN( renderWidth, MAX( 0.0f, maxes[i].x ) );
        unsigned long maxY = (unsigned long)MIN( renderHeight, MAX( 0.0f, maxes[i].y ) );
        
        if( ( minX < maxX ) || ( minY < maxY ) ) {
            scissorRects[i].x = minX;
            scissorRects[i].y = minY;
            scissorRects[i].width = maxX - minX;
            scissorRects[i].height = maxY - minY;
        
            scissorsValid[i] = ( scissorRects[i].width > 0 ) && ( scissorRects[i].height > 0 );
        
            /*if( scissorsValid[i] ) {
                llog( LOG_DEBUG, "%i: <%f, %f, %f, %f>  (%u, %u, %u, %u)", i, mins[i].x, mins[i].y, maxes[i].x, maxes[i].x, minX, minY, maxX, maxY );
            }//*/
        }
    }
}

static void drawTriangles( int cam, TriangleList* triList, id<MTLBuffer> triBuffer, id<MTLRenderCommandEncoder> commandEncoder, PipelineType pipelineType, id<MTLDepthStencilState>(*onStencilSwitch)( int ), bool testing )
{ @autoreleasepool {
    int triIdx = 0;
    ShaderType lastBoundShader = NUM_SHADERS;
    int lastSetStencil = -1;
    uint32_t camFlags;
    int triCount = 0;
    id<MTLDepthStencilState> stencilState = nonStencilPassIgnoreDSState;
    MTLScissorRect scissorRect;
    scissorRect.x = 0;
    scissorRect.y = 0;
    scissorRect.width = rendererData.renderTarget.width;
    scissorRect.height = rendererData.renderTarget.height;
    
    Uniforms uniforms;
    Matrix4 vpMat;
    cam_GetVPMatrix( cam, &vpMat );
    simd_float4 row0 = simd_make_float4( vpMat.m[0], vpMat.m[4], vpMat.m[8],  vpMat.m[12] );
    simd_float4 row1 = simd_make_float4( vpMat.m[1], vpMat.m[5], vpMat.m[9],  vpMat.m[13] );
    simd_float4 row2 = simd_make_float4( vpMat.m[2], vpMat.m[6], vpMat.m[10], vpMat.m[14] );
    simd_float4 row3 = simd_make_float4( vpMat.m[3], vpMat.m[7], vpMat.m[11], vpMat.m[15] );
    uniforms.vpMat = simd_matrix_from_rows( row0, row1, row2, row3 );
    
    do {
        id<MTLTexture> texture = (__bridge id<MTLTexture>)triList->triangles[triIdx].texture.mtlTexture;
        id<MTLTexture> extraTexture = (__bridge id<MTLTexture>)triList->triangles[triIdx].extraTexture.mtlTexture;
        float floatVal0 = triList->triangles[triIdx].floatVal0;
        triList->lastIndexBufferIndex = -1;
        
        if( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].shaderType != lastBoundShader ) ) {
            lastBoundShader = triList->triangles[triIdx].shaderType;
            camFlags = cam_GetFlags( cam );
            uniforms.floatVal0 = 0.0f;
        }
        
        if( ( triIdx <= triList->lastTriIndex ) && ( triList->triangles[triIdx].stencilGroup != lastSetStencil ) ) {
            // next stencil group
            //llog( LOG_DEBUG, "setting stencil: %i -> %i", lastSetStencil, triList->triangles[triIdx].stencilGroup );
            lastSetStencil = triList->triangles[triIdx].stencilGroup;
            //stencilState = onStencilSwitch( lastSetStencil );
            if( ( lastSetStencil >= 0 ) && scissorsValid[lastSetStencil] ) {
                scissorRect = scissorRects[lastSetStencil];
            } else {
                scissorRect.x = 0;
                scissorRect.y = 0;
                scissorRect.width = rendererData.renderTarget.width;
                scissorRect.height = rendererData.renderTarget.height;
            }
        }
        
        while( ( triIdx <= triList->lastTriIndex ) &&
              ( triList->triangles[triIdx].texture.mtlTexture == texture ) &&
              ( triList->triangles[triIdx].extraTexture.mtlTexture == extraTexture ) &&
              ( triList->triangles[triIdx].shaderType == lastBoundShader ) &&
              ( triList->triangles[triIdx].stencilGroup == lastSetStencil ) &&
              FLT_EQ( triList->triangles[triIdx].floatVal0, floatVal0 ) ) {
            
            if( ( triList->triangles[triIdx].camFlags & camFlags ) != 0 ) {
                triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[0];
                triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[1];
                triList->platformTriList.indices[++triList->lastIndexBufferIndex] = triList->triangles[triIdx].vertexIndices[2];
            }
            ++triIdx;
            ++triCount;
        }
        
        if( triList->lastIndexBufferIndex < 0 ) {
            continue;
        }
        
        uniforms.floatVal0 = floatVal0;
        
        // grab an open index buffer from the pool and fill it with the data
        size_t bufferSize = sizeof(uint32_t) * ( triList->lastIndexBufferIndex + 1 );
        id<MTLBuffer> idxBuffer = grabIndexBuffer( bufferSize );
        void* idxBufferContents = [idxBuffer contents];
        memcpy( idxBufferContents, triList->platformTriList.indices, bufferSize );//*/
        
        //if( testing ) llog( LOG_DEBUG, "num tris: %i", ( triList->lastIndexBufferIndex + 1 ) / 3 );

        [commandEncoder pushDebugGroup:@"Drawing batch"];
        [commandEncoder setCullMode:MTLCullModeNone];
        [commandEncoder setRenderPipelineState:triPipelines[lastBoundShader][pipelineType]];
        [commandEncoder setVertexBuffer:triBuffer offset:0 atIndex:0];
        [commandEncoder setVertexBytes:&uniforms length:sizeof(Uniforms) atIndex:1]; // small, don't bother with buffer
        [commandEncoder setFragmentBytes:&uniforms length:sizeof(Uniforms) atIndex:0]; // small, don't bother with buffer
        [commandEncoder setFragmentSamplerState:rendererData.sampler atIndex:0];
        [commandEncoder setFragmentTexture:texture atIndex:0];
        [commandEncoder setFragmentTexture:extraTexture atIndex:1];
        [commandEncoder setDepthStencilState:stencilState];
        [commandEncoder setStencilReferenceValue:0xFF];
        //llog( LOG_DEBUG, "Using rect: %u, %u, %u, %u", scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height );
        [commandEncoder setScissorRect:scissorRect];
        [commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                  indexCount:triList->lastIndexBufferIndex + 1
                                  indexType:MTLIndexTypeUInt32
                                  indexBuffer:idxBuffer
                                  indexBufferOffset:0];
        [commandEncoder popDebugGroup];
        
    } while( triIdx <= triList->lastTriIndex );
} }

void triPlatform_RenderForCamera( int cam, TriangleList* solidTriangles, TriangleList* transparentTriangles, TriangleList* stencilTriangles )
{ @autoreleasepool {
#ifdef ENABLE_STENCIL
    [rendererData.cmdBuffer pushDebugGroup:@"Drawing stencil buffer for camera"];
    MTLRenderPassDescriptor* stencilPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    stencilPassDesc.stencilAttachment.texture = rendererData.depthStencilTexture;
    stencilPassDesc.stencilAttachment.clearStencil = 0;
    stencilPassDesc.stencilAttachment.loadAction = MTLLoadActionClear;
    stencilPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
    id<MTLRenderCommandEncoder> stencilCommandEncoder = [rendererData.cmdBuffer renderCommandEncoderWithDescriptor:stencilPassDesc];
    [stencilCommandEncoder pushDebugGroup:@"Draw stencil triangles"];
    drawTriangles( cam, stencilTriangles, currStencilTriVertBuffer, stencilCommandEncoder, PT_STENCIL, onStencilSwitch_Stencil);
    [stencilCommandEncoder popDebugGroup];
    [stencilCommandEncoder endEncoding];
    
    [rendererData.cmdBuffer popDebugGroup]; // drawing stencil
#else
    // stencil hack, just create scissor areas based on the stencil triangles
    scissorsStencilHack( cam, stencilTriangles, rendererData.renderTarget.width, rendererData.renderTarget.height );
#endif
    
    [rendererData.cmdBuffer pushDebugGroup:@"Drawing color for camera"];
    MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    passDesc.depthAttachment.texture = rendererData.depthStencilTexture;
    passDesc.depthAttachment.clearDepth = 1.0f;
    passDesc.depthAttachment.loadAction = MTLLoadActionClear;
    passDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
#ifdef STENCIL_ENABLED
    passDesc.stencilAttachment.texture = rendererData.depthStencilTexture;
    passDesc.stencilAttachment.clearStencil = 0;
    passDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
    passDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
#endif
    passDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
#ifdef ENABLE_MSAA
    passDesc.colorAttachments[0].texture = rendererData.msaaTarget;
    passDesc.colorAttachments[0].resolveTexture = rendererData.renderTarget;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
#else
    passDesc.colorAttachments[0].texture = rendererData.renderTarget;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
#endif
    id<MTLRenderCommandEncoder> commandEncoder = [rendererData.cmdBuffer renderCommandEncoderWithDescriptor:passDesc];
    // solid
    [commandEncoder pushDebugGroup:@"Draw solid triangles"];
    drawTriangles( cam, solidTriangles, currSolidTriVertBuffer, commandEncoder, PT_SOLID, onStencilSwitch_Standard, false );
    [commandEncoder popDebugGroup];//*/
    
    // transparent
    [commandEncoder pushDebugGroup:@"Draw transparent triangles"];
    drawTriangles( cam, transparentTriangles, currTransparentTriVertBuffer, commandEncoder, PT_TRANSPARENT, onStencilSwitch_Standard, false );
    [commandEncoder popDebugGroup];//*/
    
    [commandEncoder endEncoding];
    [rendererData.cmdBuffer popDebugGroup]; // drawing for camera
} }

void triPlatform_RenderEnd( void )
{
//    assert( 0 && "Not implemented" );
}

//**********************************************************************
// debugRendering

id<MTLRenderPipelineState> debugPipeline;
// TODO: Get this working, we're ignoring it until we need it.
bool debugRendererPlatform_Init( size_t bufferSize )
{
    MTLVertexDescriptor* vertDesc = [MTLVertexDescriptor vertexDescriptor];
    vertDesc.attributes[0].format = MTLVertexFormatFloat3; // pos
    vertDesc.attributes[0].bufferIndex = 0;
    vertDesc.attributes[0].offset = 0;
    
    vertDesc.attributes[1].format = MTLVertexFormatFloat4; // color
    vertDesc.attributes[1].bufferIndex = 0;
    vertDesc.attributes[1].offset = sizeof(float) * 3;
    
    vertDesc.attributes[2].format = MTLVertexFormatFloat2; // uv
    vertDesc.attributes[2].bufferIndex = 0;
    vertDesc.attributes[2].offset = sizeof(float) * 7;
    
    vertDesc.layouts[0].stride = sizeof(Vertex);
    vertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    MTLRenderPipelineDescriptor* plDesc = [MTLRenderPipelineDescriptor new];
    plDesc.vertexFunction = [rendererData.library newFunctionWithName:@"vertex_debug"];
    plDesc.fragmentFunction = [rendererData.library newFunctionWithName:@"fragment_debug"];
    plDesc.vertexDescriptor = vertDesc;
    plDesc.colorAttachments[0].pixelFormat = rendererData.layer.pixelFormat;
    
    NSError* err = nil;
    debugPipeline = [rendererData.mtlDevice newRenderPipelineStateWithDescriptor:plDesc error:&err];
    
    SDL_assert( err == nil );
    
    return true;
}

void debugRendererPlatform_Render( DebugVertex* debugBuffer, int lastDebugVert )
{
    //assert( 0 && "Not implemented" );
}

#endif
