// draws the image using the SDF method, but defines a certain range as an outline, the outline
//  will be assumed to be white
#version 330

in vec2 vTex;
in vec4 vCol; // base color

uniform sampler2D textureUnit0;
uniform float floatVal0; // used for outline range

out vec4 outCol;

void main( void )
{
	outCol = texture2D(textureUnit0, vTex);
	outCol.a *= vCol.a;
	float dist = outCol.a;
	float edgeDist = 0.5f;
	float edgeWidth = 0.7f * fwidth( dist );
	//outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	float outlineEdge = edgeDist + floatVal0;
	float outline = smoothstep( outlineEdge - edgeWidth, outlineEdge + edgeWidth, dist );
	outCol.rgb = mix( vec3( 1.0f, 1.0f, 1.0f ), vCol.rgb, outline );
}