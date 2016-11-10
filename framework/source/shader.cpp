#include "shader.hpp"
#include "shader_loader.hpp"

vk::ShaderStageFlagBits execution_to_stage(spv::ExecutionModel const& model) {
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
    return vk::ShaderStageFlagBits::eAll;
  }
}

std::vector<vk::DescriptorSetLayout> to_set_layouts(vk::Device const& device, layout_shader_t const& shader) {
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

vk::PipelineLayout to_pipe_layout(vk::Device const& device, std::vector<vk::DescriptorSetLayout> const& set_layouts) {
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.setLayoutCount = std::uint32_t(set_layouts.size());
  pipelineLayoutInfo.pSetLayouts = set_layouts.data();

  return device.createPipelineLayout(pipelineLayoutInfo);
}

Shader::Shader()
{}

Shader::Shader(Device const& device, std::vector<std::string> const& paths)
 :m_device{&device}
 ,m_paths{paths}
 {
  std::vector<layout_module_t> module_layouts{};
  for(auto const& path : m_paths) {
    auto code = shader_loader::read_file(path);
    m_modules.emplace_back(shader_loader::module(code, device));

    // Read SPIR-V from disk or similar.
    std::vector<uint32_t> spirv_binary{};
    spirv_binary.resize(code.size() / 4);
    std::memcpy(spirv_binary.data(), code.data(), code.size() * sizeof(char));

    module_layouts.emplace_back(shader_loader::createLayout(spirv_binary));

    vk::PipelineShaderStageCreateInfo stage_info{};
    stage_info.stage = module_layouts.back().stage;
    stage_info.module = m_modules.back();
    stage_info.pName = "main";
    m_shader_stages.emplace_back(stage_info);
  }

  info() = layout_shader_t{module_layouts};
  m_set_layouts = to_set_layouts(device, info());
  get() = to_pipe_layout(device, m_set_layouts);
 }

void Shader::destroy() {
  (*m_device)->destroyPipelineLayout(get());
  get() = VK_NULL_HANDLE;
  for(auto const& module : m_modules) {
    (*m_device)->destroyShaderModule(module);
  }
  for(auto const& set_layout : m_set_layouts) {
    (*m_device)->destroyDescriptorSetLayout(set_layout);
  }
}

Shader::Shader(Shader && dev)
 :Shader{}
{
  swap(dev);
}

Shader& Shader::operator=(Shader&& dev) {
  swap(dev);
  return *this;
}

void Shader::swap(Shader& dev) {
  WrapperShader::swap(dev);
  std::swap(m_device, dev.m_device);
  std::swap(m_paths, dev.m_paths);
  std::swap(m_modules, dev.m_modules);
  std::swap(m_set_layouts, dev.m_set_layouts);
  std::swap(m_shader_stages, dev.m_shader_stages);
}

vk::PipelineLayout const& Shader::pipelineLayout() const {
  return get();
}

// std::vector<vk::ShaderModule> const& Shader::shaderModules() const {
//   return m_pipe;
// }
vk::DescriptorSetLayout const& Shader::setLayout(std::size_t index) const {
  return m_set_layouts[index];
}

std::vector<vk::DescriptorSetLayout> const& Shader::setLayouts() const {
  return m_set_layouts;
}

std::vector<vk::PipelineShaderStageCreateInfo> const& Shader::shaderStages() const {
  return m_shader_stages;
}
