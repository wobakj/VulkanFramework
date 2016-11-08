#include "shader.hpp"
#include "shader_loader.hpp"

vk::ShaderStageFlags execution_to_stage(spv::ExecutionModel const& model) {
  if (model == spv::ExecutionModelVertex) {
    return vk::ShaderStageFlagBits::eVertex;
  }
  else if (model == spv::ExecutionModelTessellationControl) {
    return vk::ShaderStageFlagBits::eTessellationControl;
  }
  else if (model == spv::ExecutionModelTessellationEvaluation) {
    return vk::ShaderStageFlagBits::eTessellationEvaluation;
  }
  else if (model == spv::ExecutionModelGeometry) {
    return vk::ShaderStageFlagBits::eGeometry;
  }
  else if (model == spv::ExecutionModelFragment) {
    return vk::ShaderStageFlagBits::eFragment;
  }
  else if (model == spv::ExecutionModelGLCompute) {
    return vk::ShaderStageFlagBits::eCompute;
  }
  else {
    throw std::runtime_error{"execution model not supported"};
    return vk::ShaderStageFlags{};
  }
}

std::vector<vk::DescriptorSetLayout> to_layouts(vk::Device const& device, layout_shader_t const& shader) {
  std::vector<vk::DescriptorSetLayout> set_layouts{};
  for(std::size_t idx_set = 0; idx_set < shader.bindings.size(); ++idx_set) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings{};
    for(auto const& pair_desc : shader.bindings[idx_set]) {
      bindings.emplace_back(pair_desc.second);
    }
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = std::uint32_t(bindings.size());
    layoutInfo.pBindings = bindings.data();
    set_layouts.emplace_back(device.createDescriptorSetLayout(layoutInfo));
  }
  return set_layouts;
}

std::vector<vk::PipelineShaderStageCreateInfo> to_stages(vk::Device const& device, layout_shader_t const& shader) {
  std::vector<vk::PipelineShaderStageCreateInfo> stages{};
  for(auto const& module : shader.modules) {
    vk::PipelineShaderStageCreateInfo stage_info{};
    stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    // stage_info.module = fragShaderModule;
    stage_info.pName = "main";

  }
  return stages;
}

vk::PipelineLayout to_pipe_layout(vk::Device const& device, layout_shader_t const& shader) {
  auto set_layouts = to_layouts(device, shader);

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.setLayoutCount = std::uint32_t(set_layouts.size());
  pipelineLayoutInfo.pSetLayouts = set_layouts.data();

  return device.createPipelineLayout(pipelineLayoutInfo);
}

Shader::Shader(Device const& device, std::vector<std::string> const& paths)
 :m_device{&device}
 ,m_paths{paths}
 {}

// Shader::Shader(Shader && dev);

// Shader& Shader::operator=(Shader&& dev);

// void Shader::swap(Shader& dev);