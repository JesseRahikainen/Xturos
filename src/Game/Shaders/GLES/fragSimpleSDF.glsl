// gotten from here: https://metalbyexample.com/rendering-text-in-metal-with-signed-distance-fields/
#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol;

uniform sampler2D textureUnit0;

out mediump vec4 outCol;

void main( void )
{
	mediump float dist = texture( textureUnit0, vTex ).a;
	const mediump float edgeDist = 0.5f;
	//float edgeWidth = 0.7f * length( vec2( dFdx( dist ), dFdy( dist ) ) );
	mediump float edgeWidth = 0.7f * fwidth( dist );
	mediump float opacity = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	outCol = vec4( vCol.r, vCol.g, vCol.b, opacity );
}