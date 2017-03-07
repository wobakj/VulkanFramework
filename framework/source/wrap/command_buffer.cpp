#include "wrap/command_buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/command_pool.hpp"
#include "wrap/pipeline.hpp"
#include "geometry.hpp"

#include <iostream>

CommandBuffer::CommandBuffer()
 :WrapperCommandBuffer{}
 ,m_device{nullptr}
 ,m_recording{false}
{}

CommandBuffer::CommandBuffer(CommandBuffer && rhs)
 :WrapperCommandBuffer{}
 ,m_device{nullptr}
 ,m_recording{false}
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
  std::swap(m_recording, rhs.m_recording);

  assert(!m_device || m_object);
  assert(!rhs.m_device || rhs.m_object);
}

void CommandBuffer::bindPipeline(Pipeline const& pipeline) {
  assert(m_recording);
  m_object.bindPipeline(pipeline.bindPoint(), pipeline);
}

void CommandBuffer::bindGeometry(Geometry const& geometry) {
  assert(m_recording);
  get().bindVertexBuffers(0, {geometry.vertices().buffer()}, {geometry.vertices().offset()});
  if (geometry.numIndices() > 0) {
    get().bindIndexBuffer(geometry.indices().buffer(), geometry.indices().offset(), vk::IndexType::eUint32);
  }
}

void CommandBuffer::begin(vk::CommandBufferBeginInfo const& info) {
  get().begin(info);
  m_recording = true;
}

void CommandBuffer::end() {
  get().end();
  m_recording = false;
}