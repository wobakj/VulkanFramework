#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_Position;
layout(location = 1) in vec3 in_Color;

out gl_PerVertex {
  vec4 gl_Position;
};

layout(location = 0) out vec3 frag_Color;

void main() {
  gl_Position = vec4(in_Position, 0.0, 1.0);
  frag_Color = in_Color;
}