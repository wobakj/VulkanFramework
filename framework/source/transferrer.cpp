#include "transferrer.hpp"

#include "wrap/memory.hpp"
#include "wrap/device.hpp"
#include "wrap/buffer.hpp"
#include "wrap/image_res.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/command_pool.hpp"
#include "wrap/command_buffer.hpp"

#include <iostream>

Transferrer::Transferrer()
 :m_device{nullptr}
{}

Transferrer::Transferrer(CommandPool& pool)
 :Transferrer{}
 {
  m_device = &pool.device();
  // create buffer for onetime commands
  m_command_buffer_help = pool.createBuffer(vk::CommandBufferLevel::ePrimary);
 }

Transferrer::Transferrer(Transferrer && dev)
 :Transferrer{}
 {
  swap(dev);
 }

Transferrer& Transferrer::operator=(Transferrer&& dev) {
  swap(dev);
  return *this;
}

void Transferrer::swap(Transferrer& dev) {
  // WrapperDevice::swap(dev);
  std::swap(m_device, dev.m_device);
  std::swap(m_command_buffer_help, dev.m_command_buffer_help);
  std::swap(m_buffer_stage, dev.m_buffer_stage);
  std::swap(m_memory_stage, dev.m_memory_stage);
}

void Transferrer::adjustStagingPool(vk::DeviceSize const& size) {
  if (!m_memory_stage || m_memory_stage->size() < size) {
    // create new staging buffer
    m_buffer_stage = std::unique_ptr<Buffer>{new Buffer{*m_device, size, vk::BufferUsageFlagBits::eTransferSrc}};
    m_memory_stage = std::unique_ptr<Memory>{new Memory{*m_device, m_buffer_stage->memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_buffer_stage->size()}};
    m_buffer_stage->bindTo(*m_memory_stage, 0);
  }
}

void Transferrer::uploadImageData(void const* data_ptr, ImageRes& image, vk::ImageLayout const& newLayout) {
  { //lock staging memory
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(image.size());
    m_buffer_stage->setData(data_ptr, image.size(), 0);
    transitionToLayout(image, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(*m_buffer_stage, image, image.info().extent.width, image.info().extent.height, image.info().extent.depth);
  }

  transitionToLayout(image, vk::ImageLayout::eTransferDstOptimal, newLayout);
}

void Transferrer::uploadBufferData(void const* data_ptr, BufferView& buffer_view) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(buffer_view.size());
    m_buffer_stage->setData(data_ptr, buffer_view.size(), 0);

    copyBuffer(m_buffer_stage->get(), buffer_view.buffer(), buffer_view.size(), 0, buffer_view.offset());
  }
}

void Transferrer::uploadBufferData(void const* data_ptr, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  uploadBufferData(data_ptr, buffer.size(), buffer, dst_offset);
}

void Transferrer::uploadBufferData(void const* data_ptr, vk::DeviceSize const& size, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(size);
    m_buffer_stage->setData(data_ptr, size, 0);

    copyBuffer(m_buffer_stage->get(), buffer.get(), size, 0, dst_offset);
  }
}

void Transferrer::copyBuffer(vk::Buffer const& srcBuffer, vk::Buffer const& dstBuffer, vk::DeviceSize const& size, vk::DeviceSize const& src_offset, vk::DeviceSize const& dst_offset) const {
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();

  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  copyRegion.srcOffset = src_offset;
  copyRegion.dstOffset = dst_offset;
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, {copyRegion});

  endSingleTimeCommands();
}

void Transferrer::copyImage(ImageRes const& srcImage, ImageRes& dstImage, uint32_t width, uint32_t height) const {
  vk::ImageSubresourceLayers subResource{};
  if (is_depth(srcImage.view().format())) {
    subResource.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(srcImage.view().format())) {
      subResource.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    subResource.aspectMask = vk::ImageAspectFlagBits::eColor;
  }
  subResource.baseArrayLayer = 0;
  subResource.mipLevel = 0;
  subResource.layerCount = srcImage.info().arrayLayers;


  vk::ImageCopy region{};
  region.srcSubresource = subResource;
  region.dstSubresource = subResource;
  region.srcOffset = vk::Offset3D{0, 0, 0};
  region.dstOffset = vk::Offset3D{0, 0, 0};
  region.extent.width = width;
  region.extent.height = height;
  region.extent.depth = 1;

  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyImage(
    srcImage, vk::ImageLayout::eTransferSrcOptimal,
    dstImage, vk::ImageLayout::eTransferDstOptimal,
    1, &region
  );
  endSingleTimeCommands();
}


void Transferrer::copyBufferToImage(Buffer const& srcBuffer, ImageView& dstImage, vk::ImageLayout imageLayout, uint32_t layer) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyBufferToImage(srcBuffer, dstImage, imageLayout, layer);
  endSingleTimeCommands();
}

void Transferrer::copyImageToBuffer(Buffer const& srcBuffer, ImageView const& dstImage, vk::ImageLayout imageLayout, uint32_t layer) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyImageToBuffer(srcBuffer, dstImage, imageLayout, layer);
  endSingleTimeCommands();
}

void Transferrer::copyBufferToImage(Buffer const& srcBuffer, ImageRes& dstImage, uint32_t width, uint32_t height, uint32_t depth) const {
  vk::ImageSubresourceLayers subResource{};
  if (is_depth(dstImage.view().format())) {
    subResource.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(dstImage.view().format())) {
      subResource.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    subResource.aspectMask = vk::ImageAspectFlagBits::eColor;
  }
  subResource.baseArrayLayer = 0;
  subResource.mipLevel = 0;
  subResource.layerCount = dstImage.info().arrayLayers;


  vk::BufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = width;
  region.bufferImageHeight = height;
  region.imageSubresource = subResource;
  region.imageOffset = vk::Offset3D{0, 0, 0};
  region.imageExtent.width = width;
  region.imageExtent.height = height;
  region.imageExtent.depth = depth;

  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyBufferToImage(
    srcBuffer,
    dstImage, vk::ImageLayout::eTransferDstOptimal,
    1, &region
  );
  endSingleTimeCommands();
}

void Transferrer::transitionToLayout(ImageRes& img, vk::ImageLayout const& newLayout) const {
  transitionToLayout(img.get(), img.info(), vk::ImageLayout::eUndefined, newLayout);
}

void Transferrer::transitionToLayout(ImageView const& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.transitionLayout(img, oldLayout, newLayout);
  endSingleTimeCommands();
}

void Transferrer::transitionToLayout(ImageRes& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const {
  transitionToLayout(img.get(), img.info(), oldLayout, newLayout);
}

void Transferrer::transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const {
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = img;

  if (is_depth(info.format)) {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(info.format)) {
      barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  }

  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = info.mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = info.arrayLayers;

  barrier.srcAccessMask = layout_to_access(oldLayout);
  barrier.dstAccessMask = layout_to_access(newLayout);

  commandBuffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
  endSingleTimeCommands();
}

CommandBuffer const& Transferrer::beginSingleTimeCommands() const {
  m_mutex_single_command.lock();
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  m_command_buffer_help->begin(beginInfo);

  return m_command_buffer_help;
}


void Transferrer::endSingleTimeCommands() const {
  m_command_buffer_help->end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_command_buffer_help.get();

  m_device->getQueue("transfer").submit({submitInfo}, nullptr);
  m_device->getQueue("transfer").waitIdle();
  m_command_buffer_help->reset({});

  m_mutex_single_command.unlock();
}

void Transferrer::deallocate() {
  m_buffer_stage.reset();
  m_memory_stage.reset();
}

Device const& Transferrer::device() const {
  return *m_device;
}
