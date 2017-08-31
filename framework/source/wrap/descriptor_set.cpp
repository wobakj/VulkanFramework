#include "wrap/descriptor_set.hpp"

DescriptorSet::DescriptorSet()
 :WrapperDescriptorSet{}
 ,m_device{}
 ,m_pool{}
{}

DescriptorSet::DescriptorSet(DescriptorSet && rhs)
 :DescriptorSet{}
{
  swap(rhs);
}

DescriptorSet::DescriptorSet(vk::Device const& device, vk::DescriptorSet&& set, DescriptorSetLayoutInfo const& info, vk::DescriptorPool const& pool)
 :WrapperDescriptorSet{std::move(set), info}
 ,m_device{device}
 ,m_pool{pool}
{}

DescriptorSet::~DescriptorSet() {
  cleanup();
}

void DescriptorSet::destroy() {
  // m_device.freeDescriptorSets(m_pool, {get()});
}

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& rhs) {
  swap(rhs);
  return *this;
}

void DescriptorSet::swap(DescriptorSet& rhs) {
  WrapperDescriptorSet::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_pool, rhs.m_pool);
}
