#ifndef SHADER_LOADER_HPP
#define SHADER_LOADER_HPP
#include "wrap/shader.hpp"

#include <vector>

namespace vk {
  class ShaderModule;
}

namespace shader_loader {
  std::vector<uint32_t> read_file(const std::string& filename);
  vk::ShaderModule module(std::vector<uint32_t> const& code, vk::Device const& device);

  layout_module_t layout(std::vector<uint32_t> const& code);
};

#endif
