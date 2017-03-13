#ifndef MATERIAL_HPP
#define MATERIAL_HPP

// #include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

// #include <tiny_obj_loader.h>
struct gpu_mat_t {
  gpu_mat_t(glm::fvec3 const& diff, uint32_t tex)
   :diffuse{diff.x, diff.y, diff.z, 0.0f}
   // ,texture{tex}
  {}

  glm::fvec4 diffuse;
  // uint32_t texture;
};

struct material_t {
material_t(glm::fvec3 const& color, std::string const& path)
 :vec_diffuse{color}
 ,tex_diffuse{path}
{}
// material_t(tinyobj::material_t const& mat)
//  :vec_diffuse{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]}
//  ,tex_diffuse{mat.diffuse_texname}
// {}
  glm::fvec3 vec_diffuse;
  std::string tex_diffuse;
};

#endif