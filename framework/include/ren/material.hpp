#ifndef MATERIAL_HPP
#define MATERIAL_HPP

// #include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

#include <map>
struct gpu_mat_t {
  gpu_mat_t(glm::fvec3 const& diff, int32_t tex)
   :diffuse{diff.x, diff.y, diff.z, 0.0f}
   ,texture{tex}
  {}

  glm::fvec4 diffuse;
  glm::fvec3 pad;
  int32_t texture;
};

// enum tex_type : uint8_t {

// }

struct material_t {
material_t()
 :vec_diffuse{0.0f}
 ,textures{}
{}

material_t(glm::fvec3 const& color, std::map<std::string, std::string> const& paths)
 :vec_diffuse{color}
 ,textures{paths}
{}
// material_t(tinyobj::material_t const& mat)
//  :vec_diffuse{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]}
//  ,tex_diffuse{mat.diffuse_texname}
// {}
  glm::fvec3 vec_diffuse;

  std::map<std::string, std::string> textures;
};

#endif