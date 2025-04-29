#version 300 es

uniform mat4 transform; // view projection matrix

layout(location = 0) in vec3 vVertex;
layout(location = 1) in vec2 vTexCoord0;
layout(location = 2) in vec4 vColor;

out vec2 vTex;
out vec4 vCol;

void main( void )
{
	vTex = vTexCoord0;
	vCol = vColor;
	gl_Position = transform * vec4( vVertex, 1.0f );
}