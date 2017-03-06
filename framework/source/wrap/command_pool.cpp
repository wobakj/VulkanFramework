#include "wrap/command_pool.hpp"

#include "wrap/device.hpp"

CommandPool::CommandPool()
 :WrapperCommandPool{}
 ,m_device{nullptr}
{}

CommandPool::CommandPool(CommandPool && CommandPool)
 :WrapperCommandPool{}
{
  swap(CommandPool);
}

CommandPool::CommandPool(Device const& device, uint32_t idx_queue, vk::CommandPoolCreateFlags const& flags)
 :CommandPool{}
{
  m_device = &device;

  m_info.queueFamilyIndex = idx_queue;
  m_info.flags = flags;
  m_object = device->createCommandPool({flags});
}

CommandPool::~CommandPool() {
  cleanup();
}

void CommandPool::destroy() {
  (*m_device)->destroyCommandPool(get());
}

CommandPool& CommandPool::operator=(CommandPool&& CommandPool) {
  swap(CommandPool);
  return *this;
}

void CommandPool::swap(CommandPool& CommandPool) {
  WrapperCommandPool::swap(CommandPool);
  std::swap(m_device, CommandPool.m_device);
}

std::vector<vk::CommandBuffer> CommandPool::createBuffers(vk::CommandBufferLevel const& level, uint32_t num) const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.setCommandPool(get());
  allocInfo.setLevel(level);
  allocInfo.setCommandBufferCount(num);
  return (*m_device)->allocateCommandBuffers(allocInfo);
}

vk::CommandBuffer CommandPool::createBuffer(vk::CommandBufferLevel const& level) const {
  return createBuffers(level, 1)[0];
}