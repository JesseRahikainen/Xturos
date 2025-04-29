#version 330

in vec2 vTex;
in vec4 vCol;

uniform sampler2D textureUnit0;

out vec4 outCol;

void main( void )
{
	outCol = texture2D(textureUnit0, vTex) * vCol;
}