#include "wrap/command_buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/command_pool.hpp"
#include "wrap/image_view.hpp"
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
  std::swap(m_session, rhs.m_session);

  assert(!m_device || m_object);
  assert(!rhs.m_device || rhs.m_object);
}

void CommandBuffer::bindPipeline(Pipeline const& pipeline) {
  assert(m_recording);
  m_object.bindPipeline(pipeline.bindPoint(), pipeline);
  m_session.pipe_layout = pipeline.layout();
  m_session.bind_point = pipeline.bindPoint();
}

void CommandBuffer::bindGeometry(Geometry const& geometry) {
  assert(m_recording);
  get().bindVertexBuffers(0, {geometry.vertices().buffer()}, {geometry.vertices().offset()});
  if (geometry.numIndices() > 0) {
    get().bindIndexBuffer(geometry.indices().buffer(), geometry.indices().offset(), vk::IndexType::eUint32);
  }

  m_session.geometry = &geometry;
}

void CommandBuffer::drawGeometry(uint32_t instanceCount, uint32_t firstInstance) {
  if (m_session.geometry->numIndices() > 0) {
    get().drawIndexed(m_session.geometry->numIndices(), instanceCount, m_session.geometry->indexOffset(), m_session.geometry->vertexOffset(), firstInstance);
  }
  else {
    get().draw(m_session.geometry->numVertices(), instanceCount, m_session.geometry->vertexOffset(), firstInstance);
  }
}

void CommandBuffer::copyImage(vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage, vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageCopy> regions) const {
  get().copyImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regions);
}

void CommandBuffer::copyImage(vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage, vk::ImageLayout dstImageLayout, vk::ImageCopy region) const {
  get().copyImage(srcImage, srcImageLayout, dstImage, dstImageLayout, {region});
}

void CommandBuffer::copyImage(ImageView const& srcImage, vk::ImageLayout srcImageLayout, ImageView const& dstImage, vk::ImageLayout dstImageLayout, vk::ImageCopy region) const {
  get().copyImage(srcImage.image(), srcImageLayout, dstImage.image(), dstImageLayout, {region});
}

void CommandBuffer::copyImage(ImageView const& srcImage, vk::ImageLayout srcImageLayout, ImageView const& dstImage, vk::ImageLayout dstImageLayout, uint32_t level) const {
  vk::ImageCopy copy{};
  copy.srcSubresource = srcImage.layers();
  copy.dstSubresource = dstImage.layers();
  copy.extent = dstImage.extent();
  get().copyImage(srcImage.image(), srcImageLayout, dstImage.image(), dstImageLayout, {copy});
}

void CommandBuffer::transitionLayout(ImageView const& view, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const {
  view.layoutTransitionCommand(get(), layout_old, layout_new);
}

void CommandBuffer::pushConstants(vk::ShaderStageFlags stage, uint32_t offset, uint32_t size, const void* pValues) {
  get().pushConstants(m_session.pipe_layout, stage, offset, size, pValues);
}

void CommandBuffer::bindDescriptorSets(uint32_t first_set, vk::ArrayProxy<const vk::DescriptorSet> sets, vk::ArrayProxy<const uint32_t> dynamic_offsets) {
  get().bindDescriptorSets(m_session.bind_point, m_session.pipe_layout, first_set, sets, dynamic_offsets);
}

void CommandBuffer::begin(vk::CommandBufferBeginInfo const& info) {
  get().begin(info);
  m_recording = true;
}

void CommandBuffer::end() {
  get().end();
  m_recording = false;
  m_session = session_t{};
}