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

// vk::Extent2D extent_2d() const;
// float aspect() const;
// vk::Viewport asViewport() const;
// vk::Rect2D asRect() const;

#endif