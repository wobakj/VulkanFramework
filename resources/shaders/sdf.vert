#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
	vec3 res;
	float time;
  mat4 projection;
  mat4 view;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 iPosition;
layout(location = 1) out vec2 fragCoord;
layout(location = 2) out vec3 iResolution;
layout(location = 3) out float iGlobalTime;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
	fragCoord = vec2(
		((inPosition.x+1)/2) * (ubo.res.x-1),
		((inPosition.y+1)/2) * (ubo.res.y-1)
	);
	iResolution = ubo.res;
	iGlobalTime = ubo.time;
  gl_Position = vec4(inPosition, 1.0);
	iPosition = inPosition;
}
