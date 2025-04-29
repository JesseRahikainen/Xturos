#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol;

uniform sampler2D textureUnit0;

out mediump vec4 outCol;

void main( void )
{
	outCol = vec4( vCol.r, vCol.g, vCol.b, texture(textureUnit0, vTex).a * vCol.a );
	if( outCol.w <= 0.0f ) {
		discard;
	}
}