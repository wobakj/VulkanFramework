#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput position;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput normal;

layout(location = 0) flat in int frag_InstanceId;

layout(location = 0) out vec4 out_Color;

struct light_t {
  vec3 position;
  float pad;
  vec3 color;
  float radius;
};
layout(set = 0, binding = 0) buffer MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normal;
} ubo;

layout(set = 1, binding = 3) buffer LightBuffer {
  light_t[] Lights;
};

void main() {
  vec3 diffuseColor = subpassLoad(color).rgb;
  out_Color = vec4(1.0);
}
