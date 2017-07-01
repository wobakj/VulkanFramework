#ifndef CONVERSIONS_HPP
#define CONVERSIONS_HPP

#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_precision.hpp>

inline vk::Offset3D offset_3d(vk::Extent3D const& ex) {
  return vk::Offset3D{int32_t(ex.width), int32_t(ex.height), std::max(int32_t(ex.depth), 1)};
}

inline vk::Extent3D extent_3d(vk::Extent2D const& ex) {
  return vk::Extent3D{ex.width, ex.height, 1};
}

inline vk::Viewport viewport(vk::Extent2D const& ex) {
  return vk::Viewport{0.0f, 0.0f, float(ex.width), float(ex.height), 0.0f, 1.0f};
}

inline vk::Rect2D rect(vk::Extent2D const& ex) {
  return vk::Rect2D{vk::Offset2D{}, ex};
}

inline vk::Extent2D extent_2d(vk::Extent3D const& ex) {
  return vk::Extent2D{ex.width, ex.height};
}


#endif