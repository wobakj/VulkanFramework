#include "shader_loader.hpp"

#include <vulkan/vulkan.hpp>
#include <spirv_cross.hpp>

#include <fstream>
#include <iostream>

namespace shader_loader {

std::vector<uint32_t> read_file(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
      throw std::runtime_error("failed to open file" + filename + "'");
  }
  size_t fileSize = (size_t) file.tellg();
  std::vector<uint32_t> buffer(fileSize / (sizeof(uint32_t) / sizeof(char)));

  file.seekg(0);
  file.read((char*)(buffer.data()), fileSize);

  file.close();

  return buffer;
}

vk::ShaderModule module(std::vector<uint32_t> const& code, vk::Device const& device) {
  vk::ShaderModuleCreateInfo createInfo{};
  // size in bytes
  createInfo.codeSize = code.size() * sizeof(uint32_t) / sizeof(char);
  createInfo.pCode = code.data();
  return device.createShaderModule(createInfo);
}

layout_module_t layout(std::vector<uint32_t> const& code) {
  spirv_cross::Compiler comp(code);
  return layout_module_t{comp};
}

};