#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
};
layout(set = 0, binding = 0) uniform MatrixBuffer {
    mat4 view;
    mat4 proj;
    mat4 model;
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

vec2 positions[4] = vec2[](
  vec2(-1.0, -1.0),
  vec2(1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0)
);

layout(location = 0) out vec2 frag_positionNdc;

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  frag_positionNdc = vec2(gl_Position * 0.5 + 0.5);
}