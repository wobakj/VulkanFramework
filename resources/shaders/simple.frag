#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec4 out_Position;
layout(location = 2) out vec4 out_Normal;

void main() {
  out_Color = texture(texSampler, frag_Texcoord);
  out_Position = vec4(frag_Position, 1.0);
  out_Normal = vec4(frag_Normal, 0.0);
}
