#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

struct material_t {
  vec4 diffuse;
};

layout(set = 2, binding = 0) buffer Materials {
  material_t[] materials;
};

layout(push_constant, offset = 4) uniform PushFragment {
  uint index;
} material;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec4 out_Position;
layout(location = 2) out vec4 out_Normal;

void main() {
  out_Color = vec4(materials[material.index].diffuse.rgb, 0.5);
  out_Position = vec4(frag_Position, 1.0);
  out_Normal = vec4(frag_Normal, 0.0);
}
