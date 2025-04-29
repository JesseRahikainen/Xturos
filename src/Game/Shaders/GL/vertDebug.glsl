#version 330

uniform mat4 transform; // model view projection matrix

layout(location = 0) in vec4 vertex;
layout(location = 2) in vec4 color;

out vec4 vertCol;

void main( void )
{
	vertCol = color;
	gl_Position = transform * vertex;
}