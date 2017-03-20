#ifndef MATERIAL_HPP
#define MATERIAL_HPP

// #include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

#include <map>
struct gpu_mat_t {
  gpu_mat_t(glm::fvec3 const& color, float metal_v, float rough_v, int32_t diff, int32_t norm, int32_t metal, int32_t rough)
   :diffuse{color.x, color.y, color.z, 0.0f}
   ,metalness{metal_v}
   ,roughness{rough_v}
   ,tex_diffuse{diff}
   ,tex_normal{norm}
   ,tex_metalness{metal}
   ,tex_roughness{rough}
  {}

  glm::fvec4 diffuse;
  float metalness;
  float roughness;
  glm::fvec2 pad;
  int32_t tex_diffuse;
  int32_t tex_normal;
  int32_t tex_metalness;
  int32_t tex_roughness;
};

// enum tex_type : uint8_t {

// }

struct material_t {
material_t()
 :diffuse{0.0f}
 ,textures{}
{}

material_t(glm::fvec3 const& color, std::map<std::string, std::string> const& paths)
 :diffuse{color}
 ,textures{paths}
{}

  glm::fvec3 diffuse;
  float metalness;
  float roughness;
  std::map<std::string, std::string> textures;
};

#endif