#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_Position;
layout(location = 1) in vec3 frag_Normal;
layout(location = 2) in vec2 frag_Texcoord;

#define ADDNA

struct material_t {
  vec4 diffuse;
  float metalness;
  float roughness;
  vec2 pad;
  int index_diff;
  int index_norm;
  int index_metal;
  int index_rough;
};

layout(set = 0, binding = 0) uniform Camera {
  mat4 ViewMatrix;
  mat4 ProjectionMatrix;
};

layout(set = 2, binding = 0) buffer Materials {
  material_t[] materials;
};

layout(constant_id = 0) const uint NUM_TEXTURES = 1;
// hardcode texture num, is limited by glslang bug
// validation bug prevents usage of spec constant
layout(set = 2, binding = 1) uniform sampler2D diffuseTextures[75];
#ifdef ADDNA
  #define BIND_ROUGH 2
  #define BIND_METAL 3
#else
  #define BIND_ROUGH 3
  #define BIND_METAL 2
#endif
layout(set = 2, binding = BIND_METAL) uniform sampler2D metalnessTextures[75];
layout(set = 2, binding = BIND_ROUGH) uniform sampler2D normalTextures[75];
layout(set = 2, binding = 4) uniform sampler2D roughnessTextures[75];

layout(push_constant) uniform PushFragment {
  layout(offset = 4) uint index;
} material;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec4 out_Position;
layout(location = 2) out vec4 out_Normal;

mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv);
vec3 perturb_normal(sampler2D normalMap, vec3 N, vec3 p, vec2 texcoord);

void main() {
  out_Position = vec4(frag_Position, 1.0);
  // color
  int index_diff = materials[material.index].index_diff;
  if (index_diff >= 0) {
    out_Color = texture(diffuseTextures[index_diff], frag_Texcoord);
    if (out_Color.a < 1.0) {
      discard;
    }
  }
  else {
    out_Color.rgb = materials[material.index].diffuse.rgb;
  }
  // normal
  int index_norm = materials[material.index].index_norm;
  if (index_norm >= 0) {
    vec3 V = normalize((inverse(ViewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz -
      frag_Position);
    // vec3 V = normalize(ubo.eyeWorldSpace.xyz - v_PositionWorldSpace);
    out_Normal.rgb = perturb_normal(normalTextures[index_norm], normalize(frag_Normal), -V, frag_Texcoord);
    // out_Normal.rgb = frag_Normal;
    // use inverted shinyness
    #ifdef ADDNA
    // set roughness
      out_Normal.a = 1.0 - texture(normalTextures[index_norm], frag_Texcoord).a;

    #endif
  }
  else {
    #ifdef ADDNA
      out_Normal.a = 1.0;
    #endif
    out_Normal.rgb = frag_Normal;
  }
  // metalness
  #ifndef ADDNA
  int index_metal = materials[material.index].index_metal;
  if (index_metal >= 0) {
    out_Color.a = texture(metalnessTextures[index_metal], frag_Texcoord).r;
  }
  else {
    out_Color.a = materials[material.index].metalness;
  }
  #else
    out_Color.a = 0.0;
  #endif
  #ifndef ADDNA
    // roughness
    int index_rough = materials[material.index].index_rough;
    if (index_rough >= 0) {
        out_Normal.a = texture(normalTextures[index_norm], frag_Texcoord).a;
    }
    else {
        out_Normal.a = materials[material.index].roughness;
    }
  #endif
    if (out_Normal.a <= 0.0 ) {
      out_Normal.a = 1.0;
    }
}

mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv) {
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx( p );
  vec3 dp2 = dFdy( p );
  vec2 duv1 = dFdx( uv );
  vec2 duv2 = dFdy( uv );

  // solve the linear system
  vec3 dp2perp = cross( dp2, N );
  vec3 dp1perp = cross( N, dp1 );
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
  return mat3( T * invmax, B * invmax, N);
}

vec3 perturb_normal(sampler2D normalMap, vec3 N, vec3 p, vec2 texcoord) {
  vec3 normalTS = normalize(texture(normalMap, texcoord).rgb * 2.0 - 1.0);
  // vec3 normalTS = normalize(texture(normalMap, texcoord).rgb * vec3(2.0, 1.0, 2.0) - vec3(1.0, 0.0, 1.0));
  //normalTS.y = -normalTS.y;
  mat3 TBN = cotangent_frame(N, p, texcoord);
  return TBN * normalTS;
}