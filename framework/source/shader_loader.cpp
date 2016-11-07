#include "shader_loader.hpp"

#include <vulkan/vulkan.hpp>

#include <fstream>

#include "spirv_cross.hpp"
#include <vector>
#include <utility>
#include <map>
#include <iostream>

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

layout_module_t createLayout(std::string const& file_path, vk::Device const& device) {
  auto code = read_file(file_path);
  // Read SPIR-V from disk or similar.
  std::vector<uint32_t> spirv_binary{};
  spirv_binary.resize(code.size() / 4);
  std::memcpy(spirv_binary.data(), code.data(), code.size() * sizeof(char));
  return createLayout(spirv_binary, device);
}  

layout_module_t createLayout(std::vector<uint32_t> const& binary, vk::Device const& device) {
  spirv_cross::Compiler comp(std::move(binary));
  return layout_module_t{comp};
}

};