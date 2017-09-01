#include "wrap/descriptor_pool.hpp"

#include "wrap/descriptor_set_layout.hpp"

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
  m_device = device;
  m_info = info;
  m_object = m_device.createDescriptorPool(info);
}

DescriptorPool::~DescriptorPool() {
  cleanup();
}

void DescriptorPool::destroy() {
  m_device.destroyDescriptorPool(get());
}

DescriptorPool& DescriptorPool::operator=(DescriptorPool&& DescriptorPool) {
  swap(DescriptorPool);
  return *this;
}

void DescriptorPool::swap(DescriptorPool& DescriptorPool) {
  WrapperDescriptorPool::swap(DescriptorPool);
  std::swap(m_device, DescriptorPool.m_device);
}

std::vector<DescriptorSet> DescriptorPool::allocate(std::vector<DescriptorSetLayout> const& layouts) const {
  vk::DescriptorSetAllocateInfo info_alloc{};
  info_alloc.descriptorPool = get();
  info_alloc.descriptorSetCount = std::uint32_t(layouts.size());
  std::vector<vk::DescriptorSetLayout> vklayouts{};
  for (auto const& layout : layouts) {
    vklayouts.emplace_back(layout.get());
  }
  info_alloc.pSetLayouts = vklayouts.data();
  auto vk_sets = m_device.allocateDescriptorSets(info_alloc);
  std::vector<DescriptorSet> sets{};
  for (size_t i = 0; i < layouts.size(); ++i) {
    sets.emplace_back(m_device, std::move(vk_sets[i]), layouts[i].info(), get());
  }
  return sets;
}
DescriptorSet DescriptorPool::allocate(DescriptorSetLayout const& layout) const {
  vk::DescriptorSetAllocateInfo info_alloc{};
  info_alloc.descriptorPool = get();
  info_alloc.descriptorSetCount = 1;
  info_alloc.pSetLayouts = &layout.get();
  return DescriptorSet{m_device, std::move(m_device.allocateDescriptorSets(info_alloc)[0]), layout.info(), get()};
}