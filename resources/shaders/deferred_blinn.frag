#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput position;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput normal;

layout(location = 0) flat in int frag_InstanceId;

layout(location = 0) out vec4 out_Color;

struct light_t {
  vec3 position;
  float intensity;
  vec3 color;
  float radius;
};
layout(set = 0, binding = 0) uniform MatrixBuffer {
    mat4 view;
    mat4 proj;
    mat4 model;
    mat4 normal;
} ubo;

layout(set = 1, binding = 3) buffer LightBuffer {
  light_t[] Lights;
};

// material
const float ks = 0.9;            // specular intensity
const float n = 20.0;            //specular exponent 

// phong diff and spec coefficient calculation in viewspace
vec2 phongDiffSpec(const vec3 position, const vec3 normal, const float n, const vec3 lightPos) {
  vec3 toLight = normalize(lightPos - position);
  float lightAngle = dot(normal, toLight);
  // if fragment is not directly lit, use only ambient light
  if (lightAngle <= 0.0) {
    return vec2(0.0);
  }

  float diffContribution = max(lightAngle, 0.0);

  vec3 toViewer = normalize(-position);
  #ifdef BLINN
    vec3 halfwayVector = normalize(toLight + toViewer);
    float reflectedAngle = dot(halfwayVector, normal);
    float specLight = pow(reflectedAngle, n);
  #else
    vec3 reflectedLight = reflect(-toLight, normal);
    float reflectedAngle = max(dot(reflectedLight, toViewer), 0.0);
    float specLight = pow(reflectedAngle, n * 0.25);
  #endif
  // fade out specular hightlights towards edge of lit region
  float a = (1.0 - lightAngle) * ( 1.0 - lightAngle);
  specLight *= 1.0 - a * a * a;

  return vec2(diffContribution, specLight);
}

void main() {
  vec3 diffuseColor = subpassLoad(color).rgb;
  vec3 frag_Position = subpassLoad(position).xyz;
  vec3 frag_Normal = subpassLoad(normal).xyz;

  vec3 pos_light = (ubo.view * vec4(Lights[frag_InstanceId].position, 1.0)).xyz;
  float dist = distance(frag_Position, pos_light);
  float radius = Lights[frag_InstanceId].radius;
  float intensity = Lights[frag_InstanceId].intensity;

  vec2 diffSpec = phongDiffSpec(frag_Position, frag_Normal, n, pos_light);
  vec3 color = Lights[frag_InstanceId].color.rgb;
  // out_Color.rgb = diffuseColor;
  // out_Color.w = 1.0;
  // return;
  out_Color = vec4(color * 0.1 * diffuseColor 
                  + color * diffuseColor * diffSpec.x
                  + color * ks * diffSpec.y, (1.0 - dist / radius) * intensity);
  // out_Color = vec4(diffuseColor, 1.0);
}
