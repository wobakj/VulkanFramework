#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_Position;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normal;
} ubo;

struct light_t {
  vec3 position;
  float pad;
  vec3 color;
  float radius;
};

layout(set = 0, binding = 3) uniform UniformBufferObject {
  light_t[6] lights;
} light_buff;

out gl_PerVertex {
  vec4 gl_Position;
};


void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_Position, 1.0);

  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}