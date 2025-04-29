// same as the simple SDF fragment shader but uses the alpha, will not support transparency in the image
//  TODO: Make the SDF based on a separate mask image
#version 330

in vec2 vTex;
in vec4 vCol;

uniform sampler2D textureUnit0;
uniform float floatVal0; // used for glow strength

out vec4 outCol;

void main( void )
{
	outCol = texture2D(textureUnit0, vTex) * vCol;
	float dist = outCol.a;
	float edgeDist = 0.5f;
	float edgeWidth = 0.7f * fwidth( dist );
	//outCol.r = 1.0f - floatVal0; // just for testing the floatVal0 uniform
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
	if( outCol.a <= 0.0f ) {
		float t = abs( dist / 0.5f ) * 2.0f;
		outCol.a = floatVal0 * pow( t, floatVal0 * 5.0f ) * 0.2f;
	}
}