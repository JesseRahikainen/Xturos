#version 330

in vec2 vTex;
in vec4 vCol;

uniform sampler2D textureUnit0;

out vec4 outCol;

void main( void )
{
	outCol = vec4( vCol.r, vCol.g, vCol.b, texture2D(textureUnit0, vTex).r * vCol.a );
	if( outCol.w <= 0.0f ) {
		discard;
	}
}