#ifndef PIXEL_DATA_HPP
#define PIXEL_DATA_HPP

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

// holds texture data and format information
struct pixel_data {
  pixel_data()
   :pixels()
   ,width{0}
   ,height{0}
   ,depth{0}
   ,format{vk::Format::eUndefined}
  {}

  pixel_data(std::vector<std::uint8_t> dat, vk::Format f, std::size_t w, std::size_t h = 1, std::size_t d = 1)
   :pixels(dat)
   ,width{w}
   ,height{h}
   ,depth{d}
   ,format{f}
  {}

  void const* ptr() const {
    return pixels.data();
  }

  std::vector<std::uint8_t> pixels;
  std::size_t width;
  std::size_t height;
  std::size_t depth;

  // channel format
  vk::Format format; 
};

#endif