#include "shader_loader.hpp"

#include <vulkan/vulkan.hpp>

#include <fstream>

namespace shader_loader {

static std::vector<char> read_file(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
  }
  size_t fileSize = (size_t) file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

vk::ShaderModule module(std::string const& file_path, vk::Device const& device) {
  auto code = read_file(file_path);
  vk::ShaderModuleCreateInfo createInfo{};
  createInfo.codeSize = code.size();
  createInfo.pCode = (uint32_t*) code.data();
  vk::ShaderModule shaderModule = device.createShaderModule(createInfo);

  return shaderModule;
}

// vk::PipelineShaderStageCreateInfo program(std::vector<std::pair<std::string, vk::ShaderStageFlagBits>> const& stages) {

//   return program;
// }
};