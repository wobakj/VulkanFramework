#ifndef LIGHT_HPP
#define LIGHT_HPP

// #include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

struct light_t {
light_t()
 :position{0.0f}
 ,intensity{0.0f}
 ,color{0.0f}
 ,radius{0.0f}
{}

light_t(glm::fvec3 const& pos, float inte, glm::fvec3 const& col, float rad)
 :position{pos}
 ,intensity{inte}
 ,color{col}
 ,radius{rad}
{}

  glm::fvec3 position;
  float intensity;
  glm::fvec3 color;
  float radius;
};

#endif