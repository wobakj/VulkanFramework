#ifndef SHADER_LOADER_HPP
#define SHADER_LOADER_HPP
#include "shader.hpp"

#include <vulkan/vulkan.hpp>
#include "spirv_cross.hpp"

#include <vector>
#include <map>

namespace shader_loader {
  std::vector<char> read_file(const std::string& filename);
  vk::ShaderModule module(std::string const& file_path, vk::Device const& device);
  vk::ShaderModule module(std::vector<char> const& code, vk::Device const& device);

  layout_module_t createLayout(std::string const& file_path);
  layout_module_t createLayout(std::vector<uint32_t> const& binary);
};

#endif
