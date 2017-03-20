#ifndef DESCRIPTOR_SET_LAYOUT_INFO_HPP
#define DESCRIPTOR_SET_LAYOUT_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

class Shader;

class DescriptorSetLayoutInfo {
 public:
  DescriptorSetLayoutInfo();
  DescriptorSetLayoutInfo(DescriptorSetLayoutInfo const& rhs);
  DescriptorSetLayoutInfo(DescriptorSetLayoutInfo&& rhs);

  void swap(DescriptorSetLayoutInfo& rhs);

  DescriptorSetLayoutInfo& operator=(DescriptorSetLayoutInfo info);

  vk::DescriptorSetLayoutCreateInfo const& get() const;
  operator vk::DescriptorSetLayoutCreateInfo const&() const;

  void setBinding(vk::DescriptorSetLayoutBinding const& binding);
  // bind samplers if vk::DescriptorType::eSampler or vk::DescriptorType::eCombinedImageSampler
  void setBinding(uint32_t binding, vk::DescriptorType const& type, vk::ArrayProxy<vk::Sampler> const& samplers, vk::ShaderStageFlags const& stages);
  void setBinding(uint32_t binding, vk::DescriptorType const& type, uint32_t count, vk::ShaderStageFlags const& stages);
  
  operator vk::DescriptorPoolCreateInfo const&() const;

 private:
  void updateReference();
  
  vk::DescriptorSetLayoutCreateInfo m_info;
  std::vector<vk::DescriptorSetLayoutBinding> m_bindings;
};

#endif