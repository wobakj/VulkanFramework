#include "wrap/descriptor_pool.hpp"

#include "wrap/device.hpp"
#include "wrap/shader.hpp"

DescriptorPool::DescriptorPool()
 :WrapperDescriptorPool{}
 ,m_device{nullptr}
{}

DescriptorPool::DescriptorPool(DescriptorPool && DescriptorPool)
 :WrapperDescriptorPool{}
 ,m_device{nullptr}
{
  swap(DescriptorPool);
}

DescriptorPool::DescriptorPool(Device const& device, DescriptorPoolInfo const& info)
 :DescriptorPool{}
{
  m_device = &device;
  m_info = info;
  m_object = (*m_device)->createDescriptorPool(info);
}

DescriptorPool::~DescriptorPool() {
  cleanup();
}

void DescriptorPool::destroy() {
  (*m_device)->destroyDescriptorPool(get());
}

DescriptorPool& DescriptorPool::operator=(DescriptorPool&& DescriptorPool) {
  swap(DescriptorPool);
  return *this;
}

void DescriptorPool::swap(DescriptorPool& DescriptorPool) {
  WrapperDescriptorPool::swap(DescriptorPool);
  std::swap(m_device, DescriptorPool.m_device);
}

std::vector<vk::DescriptorSet> DescriptorPool::allocate(Shader const& shader) const {
  vk::DescriptorSetAllocateInfo info_alloc{};
  info_alloc.descriptorPool = get();
  info_alloc.descriptorSetCount = std::uint32_t(shader.setLayouts().size());
  info_alloc.pSetLayouts = shader.setLayouts().data();
  return (*m_device)->allocateDescriptorSets(info_alloc);
}

vk::DescriptorSet DescriptorPool::allocate(Shader const& shader, uint32_t idx_set) const {
  vk::DescriptorSetAllocateInfo info_alloc{};
  info_alloc.descriptorPool = get();
  info_alloc.descriptorSetCount = 1;
  info_alloc.pSetLayouts = &shader.setLayouts().at(idx_set);
  return (*m_device)->allocateDescriptorSets(info_alloc)[0];
}
