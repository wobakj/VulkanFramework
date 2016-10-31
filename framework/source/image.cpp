#include "image.hpp"

#include <iostream>
#include "device.hpp"


static uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
  auto memProperties = device.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
  return 0;
}

Image::Image()
 :WrapperImage{}
 ,m_memory{VK_NULL_HANDLE}
 ,m_mem_info{}
 ,m_device{nullptr}
 ,m_desc_info{}
{}

Image::Image(Image && dev)
 :Image{}
{
  swap(dev);
}

// Image::Image(Device const& device, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties)
//  :Image{}
// {
//   m_device = &device;

//   info().size = size;
//   info().usage = usage;
//   info().sharingMode = vk::SharingMode::eExclusive;
//   get() = device->createImage(info());

//   auto memRequirements = device->getImageMemoryRequirements(get());

//   vk::MemoryAllocateInfo allocInfo{};
//   allocInfo.allocationSize = memRequirements.size;
//   allocInfo.memoryTypeIndex = findMemoryType(device.physical(), memRequirements.memoryTypeBits, memProperties);
//   m_memory = device->allocateMemory(allocInfo);

//   device->bindImageMemory(get(), m_memory, 0);

//   m_desc_info.buffer = get();
//   m_desc_info.offset = 0;
//   m_desc_info.range = size;
// }

Image::Image(Device const& device, std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags) 
 :Image{}
{  
  m_device = &device;

  info().imageType = vk::ImageType::e2D;
  info().extent.width = width;
  info().extent.height = height;
  info().extent.depth = 1;
  info().mipLevels = 1;
  info().arrayLayers = 1;
  info().format = format;
  info().tiling = tiling;
  info().initialLayout = vk::ImageLayout::ePreinitialized;
  info().usage = usage;
  info().sharingMode = vk::SharingMode::eExclusive;

  get() = device->createImage(info());

  vk::MemoryRequirements memRequirements = device->getImageMemoryRequirements(get());

  m_mem_info.allocationSize = memRequirements.size;
  m_mem_info.memoryTypeIndex = findMemoryType(device.physical(), memRequirements.memoryTypeBits, mem_flags);

  m_memory = device->allocateMemory(m_mem_info);

  device->bindImageMemory(get(), m_memory, 0);
}


Image::Image(Device const& device, pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout)
 :Image{}
{
  m_device = &device;
 
  vk::DeviceSize imageSize = pixel_input .width * pixel_input.height * num_channels(pixel_input.format); 

  // create staging buffer and upload data
  Image img_staging{device, pixel_input.width, pixel_input.height, pixel_input.format, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  // create storage buffer and transfer data
  Image img_store{device, pixel_input.width, pixel_input.height, pixel_input.format, vk::ImageTiling::eOptimal, usage | vk::ImageUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal};

  img_staging.setData(pixel_input.ptr(), imageSize);

  img_staging.transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);
  img_store.transitionToLayout(vk::ImageLayout::eTransferDstOptimal);
  device.copyImage(img_staging, img_store, pixel_input.width, pixel_input.height);

  img_store.transitionToLayout(layout);

  // assign filled buffer to this
  swap(img_store);
}

void Image::setData(void const* data, vk::DeviceSize const& size) {
  void* buff_ptr = (*m_device)->mapMemory(m_memory, 0, size);
  std::memcpy(buff_ptr, data, size_t(size));
  (*m_device)->unmapMemory(m_memory);
}

void Image::transitionToLayout(vk::ImageLayout const& newLayout) {
  // get current layout form creation info
  vk::ImageLayout const& oldLayout = info().initialLayout;
  
  vk::CommandBuffer commandBuffer = m_device->beginSingleTimeCommands();
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = get();
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  if (oldLayout == vk::ImageLayout::ePreinitialized && newLayout == vk::ImageLayout::eTransferSrcOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
  } else if (oldLayout == vk::ImageLayout::ePreinitialized && newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  commandBuffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
  m_device->endSingleTimeCommands(commandBuffer);
  // store new layout
  info().initialLayout = newLayout;
}


void Image::destroy() {
  (*m_device)->freeMemory(m_memory);
  (*m_device)->destroyImage(get());
}

 Image& Image::operator=(Image&& dev) {
  swap(dev);
  return *this;
 }

vk::DescriptorImageInfo const& Image::descriptorInfo() const {
  return m_desc_info;
}

void Image::writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &m_desc_info;

  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

 void Image::swap(Image& dev) {
  WrapperImage::swap(dev);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_mem_info, dev.m_mem_info);
  std::swap(m_device, dev.m_device);
  std::swap(m_desc_info, dev.m_desc_info);
 }