#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_Position;

layout(set = 0, binding = 0) uniform MatrixBuffer {
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

layout(set = 1, binding = 3) buffer LightBuffer {
  light_t[] Lights;
};

out gl_PerVertex {
  vec4 gl_Position;
};

layout(location = 0) out flat int frag_InstanceId;

void main() {
  gl_Position = ubo.proj * vec4(Lights[gl_InstanceIndex].position + in_Position * Lights[gl_InstanceIndex].radius, 1.0);
  frag_InstanceId = gl_InstanceIndex;
}