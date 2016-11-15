#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput position;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput normal;

layout(location = 0) out vec4 out_Color;

struct light_t {
  vec3 position;
  float pad;
  vec3 color;
  float radius;
};

layout(set = 0, binding = 3) buffer UniformBufferObject {
  light_t[] lights;
} light_buff;

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

  for (uint i  = 0; i < light_buff.lights.length(); ++i) {
    float dist = distance(frag_Position, light_buff.lights[i].position.xyz);
    float radius = light_buff.lights[i].radius;
    if (dist < radius) {
      vec2 diffSpec = phongDiffSpec(frag_Position, frag_Normal, n, light_buff.lights[i].position.xyz);
      vec3 color = light_buff.lights[i].color.rgb;

      out_Color += vec4(color * 0.005 * diffuseColor 
                     + color * diffuseColor * diffSpec.x
                     + color * 0.0 * ks * diffSpec.y, 1.0 - dist / radius);
    }
  }
}
