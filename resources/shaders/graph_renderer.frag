#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

struct material_t {
  vec4 diffuse;
  vec3 pad;
  int index_texture;
};

layout(set = 2, binding = 0) buffer Materials {
  material_t[] materials;
};

layout(constant_id = 0) const uint NUM_TEXTURES = 1;
// hardcode texture num, is limited by glslang bug
// validation bug prevents usage of spec constant
layout(set = 2, binding = 1) uniform sampler2D diffuseTextures[75];
layout(set = 2, binding = 2) uniform sampler2D metalnessTextures[75];
// layout(set = 2, binding = 3) uniform sampler2D normalTextures[75];
// layout(set = 2, binding = 4) uniform sampler2D roughnessTextures[75];

layout(push_constant) uniform PushFragment {
  layout(offset = 4) uint index;
} material;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec4 out_Position;
layout(location = 2) out vec4 out_Normal;

void main() {
  uint index_texture = materials[material.index].index_texture;
  // out_Color = texture(sampler2D(diffuseTextures[index_texture], diffuseSampler), frag_Texcoord);
  out_Color = texture(diffuseTextures[index_texture], frag_Texcoord);
  if (out_Color.a == 0.0) {
    discard;
  }
  // out_Color = vec4(materials[material.index].diffuse.rgb, 0.5);
  out_Position = vec4(frag_Position, 1.0);
  out_Normal = vec4(frag_Normal, 0.0);
}
