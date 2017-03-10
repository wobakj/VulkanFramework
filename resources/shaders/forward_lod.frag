#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;
layout(location = 3) flat in int frag_VertexIndex;

layout(location = 0) out vec4 out_Color;

// add set here so matches deswcriptor in lighting shader
layout(set = 0, binding = 0) uniform MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normal;
    vec4 levels;
    vec4 shade;
} ubo;

const vec3 LightPosition = vec3(1.5, 1.0, 1.0); //diffuse color
const vec3 LightDiffuse = vec3(0.95, 0.9, 0.7); //diffuse color
const vec3 LightAmbient = vec3(0.25);        //ambient color
const vec3 LightSpecular = vec3(0.9);       //specular color

layout(set = 1, binding = 2) uniform sampler2D texSampler;

layout(set = 1, binding = 1) buffer LevelBuffer {
  uint verts_per_node;
  float[] levels;
};
struct light_t {
  vec3 position;
  float pad;
  vec3 color;
  float radius;
};

layout(set = 1, binding = 3) buffer LightBuffer {
  light_t[] lights;
} light_buff;
// material
const float ks = 0.9;            // specular intensity
const float n = 20.0;            //specular exponent 

#define SIMPLE

vec3 diffuseColor() {
  if (ubo.levels.r > 0.0) {
    float val = levels[frag_VertexIndex / verts_per_node];
    if (val < 0.5) {
      return vec3(1.0, val * 2.0, 0.0);
    }
    else {
      return vec3(1.0 - (val - 0.5) * 2.0, 1.0, 0.0);
    }
  }
  else {
    return texture(texSampler, frag_Texcoord).rgb;
  }
}

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
  vec3 diffuseColor = diffuseColor();
  if (ubo.shade.r < 1.0) {
    out_Color = vec4(diffuseColor, 0.4);
    return;
  }

  #ifdef SIMPLE
      vec2 diffSpec = phongDiffSpec(frag_Position, frag_Normal, n, LightPosition);

      out_Color += vec4(LightDiffuse * 0.005 * diffuseColor 
                      + LightDiffuse * diffuseColor * diffSpec.x
                      + LightSpecular * ks * diffSpec.y, 1.0);
  #else 
    for (uint i  = 0; i < light_buff.lights.length(); ++i) {
      vec3 pos_light = (ubo.view * vec4(light_buff.lights[i].position, 1.0)).xyz;

      float dist = distance(frag_Position, pos_light);
      float radius = light_buff.lights[i].radius;
        
      if (dist < radius) {
        vec2 diffSpec = phongDiffSpec(frag_Position, frag_Normal, n, pos_light);
        vec3 color = light_buff.lights[i].color.rgb;

        out_Color += vec4(color * 0.005 * diffuseColor 
                        + color * diffuseColor * diffSpec.x
                        + color * ks * diffSpec.y, 1.0 - dist / radius);
      }
    }
    out_Color.rgb /= out_Color.w;
  #endif
  out_Color.w = 0.5;
}