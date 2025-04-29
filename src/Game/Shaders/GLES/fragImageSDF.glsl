// same as the simple SDF fragment shader but uses the alpha, will not support transparency in the image
#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol;

uniform sampler2D textureUnit0;
uniform mediump float floatVal0;

out mediump vec4 outCol;

void main( void )
{
	outCol = texture(textureUnit0, vTex) * vCol;
	mediump float dist = outCol.a;
	const mediump float edgeDist = 0.5f;
	mediump float edgeWidth = 0.7f * fwidth( dist );
	//outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	if( outCol.a <= 0.0f ) {
		mediump float t = abs( dist / 0.5f ) * 2.0f;
		outCol.a = floatVal0 * pow( t, floatVal0 * 5.0f ) * 0.2f;
	}
}