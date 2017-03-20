#include "wrap/descriptor_set_layout_info.hpp"

DescriptorSetLayoutInfo::DescriptorSetLayoutInfo()
 :m_info{}
{}

DescriptorSetLayoutInfo::DescriptorSetLayoutInfo(DescriptorSetLayoutInfo const& rhs)
 :DescriptorSetLayoutInfo{}
{
  for (auto const& binding : rhs.m_bindings) {
    setBinding(binding);
  }
}

DescriptorSetLayoutInfo::DescriptorSetLayoutInfo(DescriptorSetLayoutInfo&& rhs)
 :DescriptorSetLayoutInfo{}
{
  swap(rhs);
}

void DescriptorSetLayoutInfo::swap(DescriptorSetLayoutInfo& rhs) {
  std::swap(m_info, rhs.m_info);
  std::swap(m_bindings, rhs.m_bindings);
  updateReference();
  rhs.updateReference();
}

DescriptorSetLayoutInfo& DescriptorSetLayoutInfo::operator=(DescriptorSetLayoutInfo info) {
  swap(info);
  return *this;
}

vk::DescriptorSetLayoutCreateInfo const& DescriptorSetLayoutInfo::get() const {
  return m_info;
}

DescriptorSetLayoutInfo::operator vk::DescriptorSetLayoutCreateInfo const&() const {
  return get();
}

void DescriptorSetLayoutInfo::setBinding(uint32_t binding, vk::DescriptorType const& type, uint32_t count, vk::ShaderStageFlags const& stages) {
  vk::DescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = type;
  new_binding.descriptorCount = count;
  new_binding.stageFlags = stages;
  setBinding(new_binding);
}

void DescriptorSetLayoutInfo::setBinding(uint32_t binding, vk::DescriptorType const& type, vk::ArrayProxy<vk::Sampler> const& samplers, vk::ShaderStageFlags const& stages) {
  vk::DescriptorSetLayoutBinding new_binding{};
  new_binding.binding = binding;
  new_binding.descriptorType = type;
  new_binding.descriptorCount = samplers.size();
  new_binding.pImmutableSamplers = samplers.data();
  new_binding.stageFlags = stages;
  setBinding(new_binding);  
}

void DescriptorSetLayoutInfo::setBinding(vk::DescriptorSetLayoutBinding const& binding) {
  bool found = 0;
  for (auto& curr_binding : m_bindings) {
    if (curr_binding.binding == binding.binding) {
      curr_binding = binding;
      found = true;
      break;
    }
  }
  if (!found) {
    m_bindings.emplace_back(binding);
    updateReference();
  }
}

void DescriptorSetLayoutInfo::updateReference() {
  m_info.bindingCount = std::uint32_t(m_bindings.size());
  m_info.pBindings = m_bindings.data();
}