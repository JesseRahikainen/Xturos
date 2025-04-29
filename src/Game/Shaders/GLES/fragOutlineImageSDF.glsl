// draws the image using the SDF method, but defines a certain range as an outline, that will be
//  a secondary color
#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol; // base color

uniform sampler2D textureUnit0;
uniform mediump float floatVal0; // used for outline range

out mediump vec4 outCol;

void main( void )
{
	outCol = texture(textureUnit0, vTex);
	outCol.a *= vCol.a;
	mediump float dist = outCol.a;
	const mediump float edgeDist = 0.5f;
	mediump float edgeWidth = 0.7f * fwidth( dist );
	//outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	mediump float outlineEdge = edgeDist + floatVal0;
	mediump float outline = smoothstep( outlineEdge - edgeWidth, outlineEdge + edgeWidth, dist );
	const mediump vec3 white = vec3( 1.0f, 1.0f, 1.0f );
	outCol.rgb = mix( white, vCol.rgb, outline );
}