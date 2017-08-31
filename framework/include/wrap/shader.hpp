#ifndef SHADER_HPP
#define SHADER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_set_layout.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>

namespace spirv_cross {
  class Compiler;
  class BufferRange;
}
// vk::ShaderStageFlagBits execution_to_stage(spv::ExecutionModel const& model);

class Device;

struct layout_module_t {

  layout_module_t(spirv_cross::Compiler const& comp);
  void add_resource(std::string const& name, unsigned set, unsigned binding, unsigned num, vk::DescriptorType const& type);

  // check if descriptor is contained
  bool has_descriptor(std::string const& name);

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> sets;
  vk::PushConstantRange push_constant;
  vk::ShaderStageFlagBits stage;
};

struct layout_shader_t {
  layout_shader_t(){};
  layout_shader_t(std::vector<layout_module_t> const& mod);

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> sets;
  std::vector<vk::PushConstantRange> push_constants;
  std::vector<layout_module_t> modules;
};

// std::vector<vk::DescriptorSetLayout> to_set_layouts(vk::Device const& device, layout_shader_t const& shader);
// vk::PipelineLayout to_pipe_layout(vk::Device const& device, std::vector<vk::DescriptorSetLayout> const& set_layouts);
std::vector<vk::DescriptorPoolSize> to_pool_sizes(layout_shader_t const& shader_layout, uint32_t num);
std::vector<vk::DescriptorPoolSize> to_pool_sizes(std::map<std::string, vk::DescriptorSetLayoutBinding> const& set, uint32_t num);

using WrapperShader = Wrapper<vk::PipelineLayout, layout_shader_t>;
class Shader : public WrapperShader {
 public:
  Shader();
  Shader(Device const& device, std::vector<std::string> const& paths);
  Shader(Shader && dev);
  Shader(Shader const&) = delete;
  ~Shader();

  Shader& operator=(Shader const&) = delete;
  Shader& operator=(Shader&& dev);

  void swap(Shader& dev);

  vk::PipelineLayout const& pipelineLayout() const;
  std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStages() const;
  DescriptorSetLayout const& setLayout(std::size_t index) const;
  std::vector<DescriptorSetLayout> const& setLayouts() const;

  vk::GraphicsPipelineCreateInfo startPipelineInfo() const;

  std::vector<std::string> const& paths() const;
 private:
  void destroy() override;

  Device const* m_device;
  std::vector<std::string> m_paths;
  std::vector<vk::ShaderModule> m_modules;
  std::vector<DescriptorSetLayout> m_set_layouts;
  std::vector<vk::PipelineShaderStageCreateInfo> m_shader_stages;
};

#endif