#ifndef SHADER_LOADER_HPP
#define SHADER_LOADER_HPP
#include <vulkan/vulkan.hpp>

#include <vector>

namespace shader_loader {
  // compile shader
  vk::ShaderModule module(std::string const& file_path, vk::Device const& device);

  // create program from given list of stages and activate given varyings for transform feedback
  // vk::PipelineShaderStageCreateInfo program(std::vector<std::pair<std::string, vk::ShaderStageFlagBits>> const&);
};

#endif
