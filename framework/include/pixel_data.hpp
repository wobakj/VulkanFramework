#ifndef PIXEL_DATA_HPP
#define PIXEL_DATA_HPP

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

inline std::uint8_t num_channels(vk::Format const& format) {
  if (format == vk::Format::eR8Unorm) {
    return 1;
  }
  else if (format == vk::Format::eR8G8Unorm) {
    return 2;
  }
  else if (format == vk::Format::eR8G8B8Unorm) {
    return 3;
  }
  else if (format == vk::Format::eR8G8B8A8Unorm) {
    return 4;
  }
  else {
    throw std::logic_error("unsupported format, incorrect format");
    return 0;
  }
}

// holds texture data and format information
struct pixel_data {
  pixel_data()
   :pixels()
   ,extent{}
   ,format{vk::Format::eUndefined}
  {}

  pixel_data(std::vector<std::uint8_t> dat, vk::Format f, std::uint32_t w, std::uint32_t h = 1, std::uint32_t d = 1)
   :pixels(dat)
   ,extent{w, h, d}
   ,format{f}
  {}

  void const* ptr() const {
    return pixels.data();
  }

  std::vector<std::uint8_t> pixels;
  vk::Extent3D extent;

  // channel format
  vk::Format format; 
};

#endif