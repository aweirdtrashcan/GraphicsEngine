#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 0) out vec3 vert;
layout(location = 1) out vec3 fNormal;

layout(set = 0, binding = 0) uniform MVP {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 modelViewProjection;
} mvp;

void main() {
	gl_Position = mvp.modelViewProjection * vec4(pos, 1.0f);
	vert = pos;
	fNormal = normal;
}