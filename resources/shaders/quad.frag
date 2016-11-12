#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput position;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput normal;

layout(location = 0) out vec4 out_Color;

  const vec3 LightPosition = vec3(1.5, 1.0, 1.0); //diffuse color
  const vec3 LightDiffuse = vec3(1.0, 0.9, 0.7); //diffuse color
  const vec3 LightAmbient = vec3(0.25);        //ambient color
  const vec3 LightSpecular = vec3(1.0);       //specular color
// };

// material
const float ks = 0.9;            // specular intensity
const float n = 20.0;            //specular exponent 

// phong diss and spec coefficient calculation in viewspace
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

  vec2 diffSpec = phongDiffSpec(frag_Position, frag_Normal, n, LightPosition);

  out_Color = vec4(LightAmbient * diffuseColor 
                 + LightDiffuse * diffuseColor * diffSpec.x
                 + LightSpecular * ks * diffSpec.y, 1.0);
}
