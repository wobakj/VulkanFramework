#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

layout(set = 0, binding = 0) uniform MatrixBuffer {
    mat4 ModelMatrix;
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
    mat4 NormalMatrix;
};

out gl_PerVertex {
  vec4 gl_Position;
};

layout(location = 0) out vec3 frag_Position;
layout(location = 1) out vec3 frag_Normal;
layout(location = 2) out vec2 frag_Texcoord;

void main() {
  gl_Position = ProjectionMatrix * ViewMatrix * ModelMatrix * vec4(in_Position, 1.0);
  frag_Position = (ViewMatrix * ModelMatrix * vec4(in_Position, 1.0)).xyz;
  frag_Normal =  (NormalMatrix * vec4(in_Normal, 0.0)).xyz;
  frag_Texcoord = in_TexCoord;
}