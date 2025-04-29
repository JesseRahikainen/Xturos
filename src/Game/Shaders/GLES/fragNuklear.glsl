#version 300 es

in highp vec2 vTex;
in highp vec4 vCol;

uniform sampler2D textureUnit0;

out highp vec4 outCol;

void main( void )
{
	outCol = texture(textureUnit0, vTex) * vCol;
}