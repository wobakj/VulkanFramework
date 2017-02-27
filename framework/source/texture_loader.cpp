#include "texture_loader.hpp"

// request supported types
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_NO_LINEAR
// get extensive error on failure
#define STBI_FAILURE_USERMSG
// create implementation here
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <gli/load.hpp>
 
#include <cstdint> 
#include <cstring> 
#include <stdexcept> 

static std::string file_extension(const std::string& path) {
  if (path.find_last_of(".") != std::string::npos) {
    return path.substr(path.find_last_of(".")+1);
  }
  return std::string{};
}

namespace texture_loader {
pixel_data stb(std::string const& file_path) {
    // match to opengl representation
  stbi_set_flip_vertically_on_load(true);
  uint8_t* data_ptr;
  int width = 0;
  int height = 0;
  int format = STBI_default;
  data_ptr = stbi_load(file_path.c_str(), &width, &height, &format, STBI_rgb_alpha);

  if(!data_ptr) {
    throw std::logic_error(std::string{"stb_image: "} + file_path + " - " + stbi_failure_reason());
  }

  // determine format of image data, internal format should be sized
  std::size_t num_components = 0;
  vk::Format vk_format{vk::Format::eUndefined};
  if (format == STBI_grey) {
    num_components = 1;
    vk_format = vk::Format::eR8Unorm;
  }
  else if (format == STBI_grey_alpha) {
    num_components = 2;
    vk_format = vk::Format::eR8G8Unorm;
  }
  else if (format == STBI_rgb) {
    num_components = 3;
    vk_format = vk::Format::eR8G8B8Unorm;
  }
  else if (format == STBI_rgb_alpha) {
    num_components = 4;
    vk_format = vk::Format::eR8G8B8A8Unorm;
  }
  else {
    throw std::logic_error("stb_image: misinterpreted data, incorrect format");
  }

  std::vector<uint8_t> texture_data(width * height * num_components);
  // copy data to vector
  std::memcpy(texture_data.data(), data_ptr, texture_data.size());
  stbi_image_free(data_ptr);

  return pixel_data{texture_data, vk_format, std::uint32_t(width), std::uint32_t(height)};
}

pixel_data gli(std::string const& file_path) {
  gli::texture tex = gli::load(file_path);

  if (tex.empty()) {
    throw std::logic_error(std::string{"gli: "} + file_path + " - failed to load texture");
  }

  auto gli_format = tex.format();
  auto format_ptr = reinterpret_cast<vk::Format*>(&gli_format);

  std::vector<uint8_t> texture_data(tex.size(tex.base_level()));
  // copy data to vector
  std::memcpy(texture_data.data(), tex.data(), texture_data.size());

  return pixel_data{texture_data, *format_ptr, uint32_t(tex.extent().x), uint32_t(tex.extent().y)};
}

pixel_data file(std::string const& file_path) {
  auto extension = file_extension(file_path);
  if (extension == "dds" || extension == "ktx") {
    return gli(file_path);
  }
  else {
    return stb(file_path);
  }
};

}