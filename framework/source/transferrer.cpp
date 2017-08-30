#include "transferrer.hpp"

#include "wrap/memory.hpp"
#include "wrap/device.hpp"
#include "wrap/buffer.hpp"
#include "wrap/image_res.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/command_pool.hpp"
#include "wrap/command_buffer.hpp"
#include "allocator_static.hpp"

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
  std::swap(m_allocator_stage, dev.m_allocator_stage);
}

void Transferrer::adjustStagingPool(vk::DeviceSize const& size) {
  if (!m_buffer_stage || m_buffer_stage->size() < size) {
    // create new staging buffer
    m_buffer_stage = std::unique_ptr<Buffer>{new Buffer{*m_device, size, vk::BufferUsageFlagBits::eTransferSrc}};
    
    auto mem_type = m_device->findMemoryType(m_buffer_stage->requirements().memoryTypeBits 
                                , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    m_allocator_stage = std::unique_ptr<StaticAllocator>{new StaticAllocator{*m_device, mem_type, m_buffer_stage->requirements().size}};
    m_allocator_stage->allocate(*m_buffer_stage);
  }
}

void Transferrer::uploadImageData(void const* data_ptr, vk::DeviceSize data_size, ImageLayers const& image, vk::ImageLayout const& newLayout) {
  { //lock staging memory
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(data_size);
    std::memcpy(m_allocator_stage->map(*m_buffer_stage), data_ptr, data_size);
    transitionToLayout(image, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(*m_buffer_stage, image, vk::ImageLayout::eTransferDstOptimal);
  }

  transitionToLayout(image, vk::ImageLayout::eTransferDstOptimal, newLayout);
}

void Transferrer::uploadBufferData(void const* data_ptr, BufferRegion const& buffer_view) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(buffer_view.size());
    std::memcpy(m_allocator_stage->map(*m_buffer_stage), data_ptr, buffer_view.size());

    copyBuffer(BufferRegion{m_buffer_stage->get(), buffer_view.size()}, buffer_view);
  }
}

void Transferrer::copyBuffer(BufferRegion const& src, BufferRegion const& dst) const {
  CommandBuffer const& commandBuffer = beginSingleTimeCommands();

  commandBuffer.copyBuffer(src, dst);

  endSingleTimeCommands();
}

void Transferrer::copyBufferToImage(BufferRegion const& srcBuffer, ImageLayers const& dstImage, vk::ImageLayout imageLayout) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyBufferToImage(srcBuffer, dstImage, imageLayout);
  endSingleTimeCommands();
}

void Transferrer::copyImageToBuffer(ImageLayers const& dstImage, vk::ImageLayout imageLayout, BufferRegion const& srcBuffer) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyImageToBuffer(dstImage, imageLayout, srcBuffer);
  endSingleTimeCommands();
}


void Transferrer::transitionToLayout(ImageRange const& img, vk::ImageLayout const& newLayout) const {
  transitionToLayout(img, vk::ImageLayout::eUndefined, newLayout);
}

void Transferrer::transitionToLayout(ImageRange const& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const {
  auto const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.transitionLayout(img, oldLayout, newLayout);
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
  m_allocator_stage.reset();
}

Device const& Transferrer::device() const {
  return *m_device;
}
