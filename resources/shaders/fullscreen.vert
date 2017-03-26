#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec2 texCoord;

void main() {
  // Vertex Shader Tricks by Bill Bilodeau - AMD at GDC 14
    gl_Position = vec4 (
          float(gl_VertexIndex / 2) * 4.0 - 1.0
        , float(gl_VertexIndex % 2) * -4.0 + 1.0
        , 0.0
        , 1.0
        );
    texCoord = vec2(
          float(gl_VertexIndex / 2) * 2.0
        , float(gl_VertexIndex % 2) * 2.0);
}
