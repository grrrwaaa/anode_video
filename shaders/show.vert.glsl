#version 330
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_texCoord;

out vec2 v_uv;

void main() {
	v_uv = a_texCoord;

	gl_Position = vec4(a_position.xyz, 1.);
}