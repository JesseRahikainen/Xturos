//
//  Shaders.metal
//  Xturos
//
//  Created by Jesse Rahikainen on 8/9/21.
//

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct TCVertex {
    float2 uv;
    float4 color;
    float4 pos [[position]];
};

struct CVertex {
    float4 color;
    float4 pos [[position]];
};

struct InDebugVertex {
    float4 pos [[attribute(0)]];
    float4 color [[attribute(2)]];
};

struct InVertex {
    float3 pos [[attribute(0)]];
    float4 color [[attribute(1)]];
    float2 uv [[attribute(2)]];
};

struct Uniforms {
    simd_float4x4 vpMat;
    float floatVal0;
};

vertex TCVertex vertex_default( InVertex vert [[stage_in]],
                                 constant Uniforms &uniforms [[buffer(1)]])
{
    // so the vp transform is making the z less than zero, which is culling it from the view
    TCVertex out;
    out.pos = uniforms.vpMat * float4( vert.pos, 1.0f );
    // we're using OpenGL matrices so we have to adjust things here to match the the normalized-device coordinates
    //  OpenGL uses a cube with each component in the range [-1,1]
    //  Apple uses a rectangular solid where x and y are in the range [-1,1], but the z is in the range [0,1]
    //  so appleZ = ( oglZ + 1 ) / 2
    out.pos.z = ( out.pos.z + 1.0f ) * 0.5f;
    out.color = vert.color;
    out.uv = vert.uv;
    return out;
}

fragment float4 fragment_colorOnly( TCVertex vert [[stage_in]],
                                   constant Uniforms &uniforms [[buffer(0)]] )
{
    return vert.color;
}

fragment float4 fragment_default( TCVertex vert [[stage_in]],
                                  constant Uniforms &uniforms [[buffer(0)]],
                                  texture2d<float> image [[texture(0)]],
                                  sampler smp [[sampler(0)]])
{
    float4 out = image.sample( smp, vert.uv ) * vert.color;
    if( out.a <= 0.0f ) {
        discard_fragment();
    }
    return out;
}

fragment float4 fragment_font( TCVertex vert [[stage_in]],
                              constant Uniforms &uniforms [[buffer(0)]],
                              texture2d<float> image [[texture(0)]],
                              sampler smp [[sampler(0)]])
{
    float4 out = float4( vert.color.r, vert.color.b, vert.color.g, image.sample( smp, vert.uv ).a * vert.color.a );
    if( out.a <= 0.0f ) {
        discard_fragment();
    }
    return out;
}

fragment float4 fragment_simpleSDF( TCVertex vert [[stage_in]],
                             constant Uniforms &uniforms [[buffer(0)]],
                             texture2d<float> image [[texture(0)]],
                             sampler smp [[sampler(0)]])
{
    float dist = image.sample( smp, vert.uv ).a;
    float edgeDist = 0.5f;
    float edgeWidth = 0.7f * fwidth( dist );
    float opacity = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
    return float4( vert.color.r, vert.color.g, vert.color.b, opacity );
}

// same as the simpleSDF but uses the alpha, will not support transparency in the image
fragment float4 fragment_imageSDF( TCVertex vert [[stage_in]],
                                  constant Uniforms &uniforms [[buffer(0)]],
                                  texture2d<float> image [[texture(0)]],
                                  sampler smp [[sampler(0)]])
{
    float4 out = image.sample( smp, vert.uv ) * vert.color;
    
    float dist = out.a;
    float edgeDist = 0.5f;
    float edgeWidth = 0.7f * fwidth( dist );
    out.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
    
    if( out.a <= 0.0f ) {
        float t = abs( dist / 0.5f ) * 2.0f;
        out.a = uniforms.floatVal0 * pow( t, uniforms.floatVal0 * 5.0f ) * 0.2f;
    }
    
    return out;
}

fragment float4 fragment_outlinedImageSDF( TCVertex vert [[stage_in]],
                                          constant Uniforms &uniforms [[buffer(0)]],
                                          texture2d<float> image [[texture(0)]],
                                          sampler smp [[sampler(0)]])
{
    float4 out = image.sample( smp, vert.uv );
    out.a *= vert.color.a;
    
    float dist = out.a;
    float edgeDist = 0.5f;
    float edgeWidth = 0.7f * fwidth( dist );
    out.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
    
    float outlineEdge = edgeDist + uniforms.floatVal0;
    float outline = smoothstep( outlineEdge - edgeWidth, outlineEdge + edgeWidth, dist );
    
    out.rgb = mix( float3( 1.0f, 1.0f, 1.0f ), vert.color.rgb, outline );
    return out;
}

fragment float4 fragment_SDFAlphaMap( TCVertex vert [[stage_in]],
                                     constant Uniforms &uniforms [[buffer(0)]],
                                     texture2d<float> image [[texture(0)]],
                                     texture2d<float> sdfImage [[texture(1)]],
                                     sampler imgSmp [[sampler(0)]] )
{
    float4 color = image.sample( imgSmp, vert.uv );
    float dist = sdfImage.sample( imgSmp, vert.uv ).a;
    float edgeDist = 0.5f;
    float edgeWidth = 0.7f * fwidth( dist );
    color.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
    return color;
}

// ***************************
//  Debug shaders
vertex CVertex vertex_debug( InDebugVertex vert [[stage_in]],
                              constant Uniforms &uniforms [[buffer(0)]])
{
    CVertex out;
    
    out.color = vert.color;
    out.pos = uniforms.vpMat * vert.pos;
    
    return out;
}

fragment float4 fragment_debug( CVertex vert [[stage_in]])
{
    return vert.color;
}
