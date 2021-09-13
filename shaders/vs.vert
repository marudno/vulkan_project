#version 450

layout(location = 0) in vec3 inPosition; //loc0 - 16B, in-input, vec3 - wektor, position - nazwa
layout(location = 1) in vec4 inColor;

void main()
{
	gl_Position = vec4(inPosition, 1.0f);
}
