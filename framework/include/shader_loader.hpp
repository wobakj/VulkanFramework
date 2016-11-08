#ifndef SHADER_LOADER_HPP
#define SHADER_LOADER_HPP
#include "shader.hpp"

#include <vulkan/vulkan.hpp>
#include "spirv_cross.hpp"

#include <vector>
#include <map>

namespace shader_loader {
  // compile shader
  std::vector<char> read_file(const std::string& filename);
  vk::ShaderModule module(std::string const& file_path, vk::Device const& device);
  vk::ShaderModule module(std::vector<char> const& code, vk::Device const& device);

  layout_module_t createLayout(std::string const& file_path, vk::Device const& device);
  layout_module_t createLayout(std::vector<uint32_t> const& binary, vk::Device const& device);

  // create program from given list of stages and activate given varyings for transform feedback
};

#endif
