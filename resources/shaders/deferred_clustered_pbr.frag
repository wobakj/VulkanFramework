#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput color;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput position;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput normal;
// vertex input
layout(location = 0) in vec2 frag_positionNdc;
// fragment output
layout(location = 0) out vec4 out_Color;

layout(set = 0, binding = 0) buffer MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normal;
    vec4 eye_world_space;
} ubo;

struct light_t {
  vec3 position;
  float pad;
  vec3 color;
  float radius;
};


layout(set = 1, binding = 4) uniform usampler3D volumeLight;

layout(set = 1, binding = 3) buffer LightBuffer {
  uvec4 lightGridSize;
  light_t[] Lights;
};

#define M_PI 3.1415926535897932384626433832795
#define INV_4_PI 0.07957747154594766788

float saturate(float x) { return clamp(x, 0.0f, 1.0f); }

// diffuse
vec3 lambert(vec3 col)
{
  return col / 3.14159265;
}

mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
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

vec3 perturb_normal(sampler2D normalMap, vec3 N, vec3 p, vec2 texcoord)
{
  vec3 normalTS = normalize(texture(normalMap, texcoord).rgb*2.0-1.0);
  //normalTS.y = -normalTS.y;
  mat3 TBN = cotangent_frame(N, p, texcoord);
  return TBN * normalTS;
}

// for microsoft BRDFs (microsurface normal m == h the half vector)
// F_schlick(F0,l,h) = F0 + (1 - F0)*(1-dot(l,h))^5

// From s2013_pbs_rad_notes.pdf
// ===============================================================================
// Calculates the Fresnel factor using Schlick’s approximation
// ===============================================================================
vec3 Fresnel(vec3 specAlbedo, vec3 h, vec3 l)
{
  float lDotH = saturate(dot(l, h));
  //return specAlbedo + (1.0f - specAlbedo) * pow((1.0f - lDotH), 5.0f);
  // see http://seblagarde.wordpress.com/2012/06/03/spherical-gaussien-approximation-for-blinn-phong-phong-and-fresnel/
  // pow(1-lDotH, 5) = exp2((-5.55473 * ldotH - 6.98316) * ldotH)
  return specAlbedo + ( saturate( 50.0 * specAlbedo.g ) - specAlbedo ) * exp2( (-5.55473 * lDotH - 6.98316) * lDotH );
}

float D_GGX(float roughness, float nDotH)
{
  float m = roughness*roughness;
  float m2 = m*m;
  float denom = nDotH * (nDotH * m2 - nDotH) + 1.0f;
  return m2 / (3.14159265 * denom * denom);
  //return m2 / (denom * denom);
}

float visibility_schlick(float roughness, float nDotV, float nDotL)
{
  float k = roughness * roughness * 0.5f;
  float g_v = nDotV * (1-k)+k;
  float g_l = nDotL * (1-k)+k;
  return 0.25f / (g_v * g_l);
}

float D_and_Vis(float roughness, vec3 N, vec3 H, vec3 V, vec3 L)
{
  float nDotL = saturate(dot(N,L));
  float nDotH = saturate(dot(N,H));
  float nDotV = max(dot(N,V), 0.0001f);
  float lDotH = saturate(dot(L,H));

  float D   = D_GGX(roughness, nDotH);
  float Vis = visibility_schlick(roughness, nDotV, nDotL);
  return D*Vis;
}

#if 0
float sRGB_to_linear(float c)
{
  if(c < 0.04045)
    return (c < 0.0) ? 0.0: c * (1.0 / 12.92);
  else
    return pow((c + 0.055)/1.055, 2.4);
}
#endif

vec3 sRGB_to_linear(vec3 c)
{
  return mix(vec3(c * (1.0 / 12.92)),
             pow((c + 0.055)/1.055, vec3(2.4)),
             greaterThanEqual(c, vec3(0.04045)));
}

void main() {
  vec3 P = subpassLoad(position).xyz;
  vec3 N = normalize(subpassLoad(normal).xyz);
  vec3 E = ubo.eye_world_space.xyz;
  vec3 V = normalize(E - P);

  uint mask_lights = 0;
  // iterate over all depth slices of the light grid and write all relevant
  // light indices into the mask
  for (uint slice = 0; slice < lightGridSize.z; ++slice) {
    ivec3 cell_index = ivec3(frag_positionNdc.x * lightGridSize.x,
                             frag_positionNdc.y * lightGridSize.y,
                             slice);
    mask_lights |= texelFetch(volumeLight, cell_index, 0).r;
  }

  for (uint i = 0u; i < Lights.length(); ++i) {
    uint flag_curr = 1 << i;
    // skip if light is not in mask
    if ((mask_lights & flag_curr) != flag_curr) continue;

    // point light calculation
    vec3 lightPosition = Lights[i].position;
    float lradius = Lights[i].radius;
    float ldist = distance(P, lightPosition);
    float att = clamp(1.0 - ldist*ldist/(lradius*lradius), 0.0, 1.0);
    att *= att;
    float intensity = 50.0; // TODO: add to light buffer and load from it here
    vec3 Cl = att * intensity * Lights[i].color * INV_4_PI;
    vec3 L = normalize(lightPosition - P);
    float NdotL = max(dot(L, N), 0.0);
    vec3 H = normalize(L + V);

    // shading
    vec3 albedo = subpassLoad(color).rgb;
    float metalness = 0.0;  // TODO: load from material
    float roughness = 1.0;  // TODO: load from material
    vec3 cspec = mix(vec3(0.04), albedo, metalness);
    vec3 cdiff = mix(albedo, vec3(0.0),  metalness);
    vec3 F = Fresnel(cspec, H, L);
    vec3 D_Vis = vec3(D_and_Vis(roughness, N, H, V, L));
    vec3 diffuse = lambert(cdiff);
    vec3 brdf = mix(diffuse, D_Vis, F);
    vec3 col = Cl * brdf * NdotL;

    out_Color += vec4(col, 1.0);
  }
}