#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	vec3 res;
	float time;
  mat4 projection;
  mat4 view;
} ubo;

layout(location = 0) out vec3 iPosition;
layout(location = 1) out vec2 fragCoord;
layout(location = 2) out vec3 iResolution;
layout(location = 3) out float iGlobalTime;

out gl_PerVertex {
  vec4 gl_Position;
};

vec2 positions[4] = vec2[](
  vec2(-1.0, -1.0),
  vec2(1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0)
);

void main() {
	iResolution = ubo.res;
	iGlobalTime = ubo.time;
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	iPosition = vec3(positions[gl_VertexIndex], 0.0);
}
