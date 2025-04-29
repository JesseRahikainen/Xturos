#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol;

uniform sampler2D textureUnit0;

out mediump vec4 outCol;

void main( void )
{
	outCol = texture(textureUnit0, vTex) * vCol;
	if( outCol.w <= 0.0f ) {
		discard;
	}
}