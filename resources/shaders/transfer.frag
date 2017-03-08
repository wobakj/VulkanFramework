#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_positionNdc;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 out_Color;

void main() {
  out_Color = vec4(0.0);
  out_Color = vec4(texture(texSampler, frag_positionNdc).rgb, 0.5);
  // out_Color = vec4(0.5);
}
