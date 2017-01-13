#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in flat int frag_InstanceId;

layout(location = 0) out vec4 out_Color;

void main() {
  out_Color = vec4(1.0, 0.0, 0.0, 1.0);
}