// gotten from here: https://metalbyexample.com/rendering-text-in-metal-with-signed-distance-fields/
#version 330

in vec2 vTex;
in vec4 vCol;

uniform sampler2D textureUnit0;

out vec4 outCol;

void main( void )
{
	float dist = texture2D( textureUnit0, vTex ).r;
	float edgeDist = 0.5f;
	//float edgeWidth = 0.7f * length( vec2( dFdx( dist ), dFdy( dist ) ) );
	float edgeWidth = 0.7f * fwidth( dist );
	float opacity = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	outCol = vec4( vCol.r, vCol.g, vCol.b, opacity );
}