#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

struct material_t {
  vec4 diffuse;
  vec3 pad;
  uint index_texture;
};

layout(set = 2, binding = 0) buffer Materials {
  material_t[] materials;
};

layout(constant_id = 0) const uint NUM_TEXTURES = 1;
// const uint bla = 24;
// layout(set = 2, binding = 1) uniform texture2D diffuseTextures[24];
layout(set = 2, binding = 1) uniform sampler2D diffuseTextures[24];
// layout(set = 2, binding = 2) uniform sampler diffuseSampler;

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
