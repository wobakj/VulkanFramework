#include "wrap/command_buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/buffer.hpp"
#include "wrap/command_pool.hpp"
#include "wrap/image_view.hpp"
#include "wrap/pipeline.hpp"
#include "geometry.hpp"

#include <iostream>

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout);

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

void CommandBuffer::copyBuffer(BufferRegion const& src, BufferRegion const& dst) const {
  vk::BufferCopy copyRegion{};
  copyRegion.size = src.size();
  copyRegion.srcOffset = src.offset();
  copyRegion.dstOffset = dst.offset();
  get().copyBuffer(src.buffer(), dst.buffer(), {copyRegion});
}

void CommandBuffer::copyImage(ImageLayers const& srcImage, vk::ImageLayout srcImageLayout, ImageLayers const& dstImage, vk::ImageLayout dstImageLayout) const {
  vk::ImageCopy copy{};
  copy.srcSubresource = srcImage;
  copy.dstSubresource = dstImage;
  copy.extent = srcImage.extent();
  get().copyImage(srcImage.image(), srcImageLayout, dstImage.image(), dstImageLayout, {copy});
}

void CommandBuffer::copyBufferToImage(BufferRegion const& srcBuffer, ImageLayers const& dstImage, vk::ImageLayout imageLayout) const {
  vk::BufferImageCopy region = buffer_image_copy(srcBuffer, dstImage);
  get().copyBufferToImage(
    srcBuffer.buffer(),
    dstImage.image(),
    imageLayout,
    1, &region
  );
}


void CommandBuffer::copyImageToBuffer(ImageLayers const& dstImage, vk::ImageLayout imageLayout, BufferRegion const& srcBuffer) const {
  vk::BufferImageCopy region{};
  region.bufferOffset = srcBuffer.offset();
  // assume tightly packed
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource = dstImage;
  region.imageExtent = dstImage.extent();
  region.imageOffset = dstImage.offset();

  get().copyImageToBuffer(
    dstImage.image(),
    imageLayout,
    srcBuffer.buffer(),
    1, &region
  );
}

void CommandBuffer::transitionLayout(ImageRange const& view, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const {
 vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = layout_old;
  barrier.newLayout = layout_new;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = view.image();
  barrier.subresourceRange = view;

  barrier.srcAccessMask = layout_to_access(layout_old);
  barrier.dstAccessMask = layout_to_access(layout_new);

  get().pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
}

void CommandBuffer::imageBarrier(ImageRange const& range, vk::ImageLayout layout, vk::PipelineStageFlags stage_src, vk::AccessFlags const& acc_src, vk::PipelineStageFlags stage_dst, vk::AccessFlags const& acc_dst) const {
 vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = layout;
  barrier.newLayout = layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = range.image();
  barrier.subresourceRange = range;

  barrier.srcAccessMask = acc_src;
  barrier.dstAccessMask = acc_dst;

  get().pipelineBarrier(
    stage_src,
    stage_dst,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
}


// TODO: check if validation allows region size of 0
void CommandBuffer::bufferBarrier(BufferRegion const& region, vk::PipelineStageFlags stage_src, vk::AccessFlags const& acc_src, vk::PipelineStageFlags stage_dst, vk::AccessFlags const& acc_dst) const {
  vk::BufferMemoryBarrier barrier{};
  barrier.buffer = region.buffer();
  barrier.size = region.size();
  barrier.offset = region.offset();
  barrier.srcAccessMask = acc_src;
  barrier.dstAccessMask = acc_dst;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  get().pipelineBarrier(
    stage_src,
    stage_dst,
    vk::DependencyFlags{},
    {},
    {barrier},
    {}
  );
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

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout) {
  if (layout == vk::ImageLayout::ePreinitialized) {
    return vk::AccessFlagBits::eHostWrite;
  }
  else if (layout == vk::ImageLayout::eUndefined) {
    return vk::AccessFlags{};
  }
  else if (layout == vk::ImageLayout::eTransferDstOptimal) {
    return vk::AccessFlagBits::eTransferWrite;
  }
  else if (layout == vk::ImageLayout::eTransferSrcOptimal) {
    return vk::AccessFlagBits::eTransferRead;
  } 
  else if (layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  } 
  else if (layout == vk::ImageLayout::eColorAttachmentOptimal) {
    return vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  } 
  else if (layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    // read but not from input attachment
    return vk::AccessFlagBits::eShaderRead;
  } 
  else if (layout == vk::ImageLayout::ePresentSrcKHR) {
    return vk::AccessFlagBits::eMemoryRead;
  } 
  else if (layout == vk::ImageLayout::eGeneral) {
    return vk::AccessFlagBits::eInputAttachmentRead
         | vk::AccessFlagBits::eShaderRead
         | vk::AccessFlagBits::eShaderWrite
         | vk::AccessFlagBits::eColorAttachmentRead
         | vk::AccessFlagBits::eColorAttachmentWrite
         | vk::AccessFlagBits::eDepthStencilAttachmentRead
         | vk::AccessFlagBits::eDepthStencilAttachmentWrite
         | vk::AccessFlagBits::eTransferRead
         | vk::AccessFlagBits::eTransferWrite
         | vk::AccessFlagBits::eHostRead
         | vk::AccessFlagBits::eHostWrite
         | vk::AccessFlagBits::eMemoryRead
         | vk::AccessFlagBits::eMemoryWrite;
  } 
  else {
    throw std::invalid_argument("unsupported layout for access mask!");
    return vk::AccessFlags{};
  }
}

vk::BufferImageCopy buffer_image_copy(BufferRegion const& buffer, ImageLayers const& image) {
  vk::BufferImageCopy region{};
  region.bufferOffset = buffer.offset();
  // assume tightly packed
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource = image;
  region.imageExtent = image.extent();
  region.imageOffset = image.offset();
  return region;
}