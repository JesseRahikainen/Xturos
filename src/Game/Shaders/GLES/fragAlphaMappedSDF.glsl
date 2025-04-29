#version 300 es

in mediump vec2 vTex;
in mediump vec4 vCol;

uniform sampler2D textureUnit0; // image color
uniform sampler2D textureUnit1; // image sdf alpha mask
uniform mediump float floatVal0; // used for glow strength

out mediump vec4 outCol;

void main( void )
{
	outCol = texture(textureUnit0, vTex) * vCol;
	mediump float dist = texture(textureUnit1, vTex).a;
	const mediump float edgeDist = 0.5f;
	mediump float edgeWidth = 0.7f * fwidth( dist );
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
}