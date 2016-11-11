#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_Texcoord;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
  // outColor = vec4(frag_Color, 1.0);
  // outColor = vec4(frag_Texcoord, 1.0, 1.0);
  outColor = texture(texSampler, frag_Texcoord);
}