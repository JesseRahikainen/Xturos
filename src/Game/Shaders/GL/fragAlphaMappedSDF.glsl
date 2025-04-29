#version 330

in vec2 vTex;
in vec4 vCol;

uniform sampler2D textureUnit0; // image color
uniform sampler2D textureUnit1; // image sdf alpha mask
uniform float floatVal0; // used for glow strength

out vec4 outCol;

void main( void )
{
   outCol = texture2D(textureUnit0, vTex) * vCol;
   float dist = texture2D(textureUnit1, vTex).a;\
	float edgeDist = 0.5f;
	float edgeWidth = 0.7f * fwidth( dist );
	outCol.a = smoothstep( edgeDist - edgeWidth, edgeDist + edgeWidth, dist );
}