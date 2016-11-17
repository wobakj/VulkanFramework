#ifndef SHADER_HPP
#define SHADER_HPP

#include "device.hpp"
#include "wrapper.hpp"

#include "spirv_cross.hpp"
#include <vulkan/vulkan.hpp>

#include <map>
#include <vector>

// vk::ShaderStageFlagBits execution_to_stage(spv::ExecutionModel const& model);

struct layout_module_t {

  layout_module_t(spirv_cross::Compiler const& comp);
  void add_resource(std::string const& name, unsigned set, unsigned binding, unsigned num, vk::DescriptorType const& type);

  // check if descriptor is contained
  bool has_descriptor(std::string const& name);

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> bindings;
  vk::ShaderStageFlagBits stage;
};

struct layout_shader_t {
  layout_shader_t(){};
  layout_shader_t(std::vector<layout_module_t> const& mod);

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> bindings;
  std::vector<layout_module_t> modules;
};

// std::vector<vk::DescriptorSetLayout> to_set_layouts(vk::Device const& device, layout_shader_t const& shader);
// vk::PipelineLayout to_pipe_layout(vk::Device const& device, std::vector<vk::DescriptorSetLayout> const& set_layouts);

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
  vk::DescriptorSetLayout const& setLayout(std::size_t index) const;
  std::vector<vk::DescriptorSetLayout> const& setLayouts() const;

  vk::DescriptorPool createPool(uint32_t max_sets = 1) const;
  vk::GraphicsPipelineCreateInfo startPipelineInfo() const;
 private:
  void destroy() override;

  Device const* m_device;
  std::vector<std::string> m_paths;
  std::vector<vk::ShaderModule> m_modules;
  std::vector<vk::DescriptorSetLayout> m_set_layouts;
  std::vector<vk::PipelineShaderStageCreateInfo> m_shader_stages;
};

#endif