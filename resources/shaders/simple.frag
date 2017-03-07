#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
// add set here so matches deswcriptor in lighting shader
layout(set = 0, binding = 0) buffer MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normal;
} ubo;
// put in buffer so set is compatible with lod set
layout(set = 1, binding = 1) buffer LevelBuffer {
  uint verts_per_node;
  float[] levels;
};

layout(push_constant) uniform Push1 {
  vec3 diffuse;
} color_override;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec4 out_Position;
layout(location = 2) out vec4 out_Normal;

void main() {
  out_Color = vec4(texture(texSampler, frag_Texcoord).rgb, 0.5);
  out_Position = vec4(frag_Position, 1.0);
  out_Normal = vec4(frag_Normal, 0.0);
  out_Color.rgb = color_override.diffuse;
}
