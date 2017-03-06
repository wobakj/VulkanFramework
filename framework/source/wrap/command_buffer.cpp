#include "wrap/command_buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/command_pool.hpp"
#include <iostream>

CommandBuffer::CommandBuffer()
 :WrapperCommandBuffer{}
 ,m_device{nullptr}
{}

CommandBuffer::CommandBuffer(CommandBuffer && rhs)
 :WrapperCommandBuffer{}
 ,m_device{nullptr}
{
  swap(rhs);
}

CommandBuffer::CommandBuffer(CommandPool const& pool, uint32_t idx_queue, vk::CommandBufferLevel const& level)
 :CommandBuffer{}
{
  m_device = &pool.device();
  m_info.setCommandPool(pool);
  m_info.setLevel(level);
  m_info.setCommandBufferCount(1);
  m_object = (*m_device)->allocateCommandBuffers(m_info)[0];
}

CommandBuffer::CommandBuffer(Device const& device, vk::CommandBuffer&& buffer, vk::CommandBufferAllocateInfo const& info)
 :CommandBuffer{}
{
  m_device = &device;
  m_object = std::move(buffer);
  m_info = info;
}

CommandBuffer::~CommandBuffer() {
  cleanup();
}

void CommandBuffer::destroy() {
  (*m_device)->freeCommandBuffers(m_info.commandPool, {get()});
}

CommandBuffer& CommandBuffer::operator=(CommandBuffer&& rhs) {
  swap(rhs);
  return *this;
}

void CommandBuffer::swap(CommandBuffer& rhs) {
  WrapperCommandBuffer::swap(rhs);
  std::swap(m_device, rhs.m_device);

  assert(!m_device || m_object);
  assert(!rhs.m_device || rhs.m_object);
}
