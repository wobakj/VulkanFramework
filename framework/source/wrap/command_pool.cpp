#include "wrap/command_pool.hpp"

#include "wrap/device.hpp"
#include "wrap/command_buffer.hpp"

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
  m_object = device->createCommandPool(info());
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

std::vector<CommandBuffer> CommandPool::createBuffers(vk::CommandBufferLevel const& level, uint32_t num) const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.setCommandPool(get());
  allocInfo.setLevel(level);
  allocInfo.setCommandBufferCount(num);
  auto buffers = (*m_device)->allocateCommandBuffers(allocInfo);
  std::vector<CommandBuffer> buffers_out{};
  for (auto& buffer : buffers) {
    buffers_out.emplace_back(std::move(CommandBuffer{*m_device, std::move(buffer), allocInfo}));
  }
  return buffers_out;
}

CommandBuffer CommandPool::createBuffer(vk::CommandBufferLevel const& level) const {
  auto buffer = CommandBuffer{};
  buffer.swap(createBuffers(level, 1)[0]); 
  return std::move(buffer);
}

Device const& CommandPool::device() const {
  return *m_device;
}
