#ifndef GPU_CAMERA_HPP
#define GPU_CAMERA_HPP

// #include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

struct gpu_camera_t {
gpu_camera_t()
 :view{1.0f}
 ,projection{1.0f}
{}

gpu_camera_t(glm::fmat4 const& v, glm::fmat4 const& p)
 :view{v}
 ,projection{p}
{}

  glm::fmat4 view;
  glm::fmat4 projection;
};

#endif