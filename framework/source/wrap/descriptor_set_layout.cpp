#include "wrap/descriptor_set_layout.hpp"

#include "wrap/device.hpp"

DescriptorSetLayout::DescriptorSetLayout()
 :WrapperDescriptorSetLayout{}
 ,m_device{nullptr}
{}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout && DescriptorSetLayout)
 :WrapperDescriptorSetLayout{}
 ,m_device{nullptr}
{
  swap(DescriptorSetLayout);
}

DescriptorSetLayout::DescriptorSetLayout(vk::Device const& device, DescriptorSetLayoutInfo const& info)
 :DescriptorSetLayout{}
{
  m_device = device;
  m_info = info;
  m_object = m_device.createDescriptorSetLayout(info);
}

DescriptorSetLayout::~DescriptorSetLayout() {
  cleanup();
}

void DescriptorSetLayout::destroy() {
  m_device.destroyDescriptorSetLayout(get());
}

DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& DescriptorSetLayout) {
  swap(DescriptorSetLayout);
  return *this;
}

void DescriptorSetLayout::swap(DescriptorSetLayout& DescriptorSetLayout) {
  WrapperDescriptorSetLayout::swap(DescriptorSetLayout);
  std::swap(m_device, DescriptorSetLayout.m_device);
}
