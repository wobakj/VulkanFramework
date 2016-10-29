#include "buffer.hpp"

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

Buffer::Buffer()
 :WrapperBuffer{}
 ,m_memory{VK_NULL_HANDLE}
 ,m_mem_info{}
 ,m_device{nullptr}
{}

Buffer::Buffer(Buffer && dev)
 :Buffer{}
{
  swap(dev);
}

Buffer::Buffer(Device const& device, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties)
 :Buffer{}
{
  m_device = &device;

  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  get() = device->createBuffer(bufferInfo);

  auto memRequirements = device->getBufferMemoryRequirements(get());

  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(device.physical(), memRequirements.memoryTypeBits, memProperties);
  m_memory = device->allocateMemory(allocInfo);

  device->bindBufferMemory(get(), m_memory, 0);
}

Buffer::Buffer(Device const& device, void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) 
 :Buffer{}
{
  m_device = &device;
  // create staging buffer and upload data
  Buffer buffer_stage{device, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  
  void* buff_ptr = device->mapMemory(buffer_stage.m_memory, 0, size);
  std::memcpy(buff_ptr, data, (size_t) size);
  device->unmapMemory(buffer_stage.m_memory);
  // create storage buffer and transfer data
  Buffer buffer_store{device, size, usage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal};

  device.copyBuffer(buffer_stage.get(), buffer_store.get(), size);
  // assign filled buffer to this
  swap(buffer_store);
}

void Buffer::destroy() {
  (*m_device)->freeMemory(m_memory);
  (*m_device)->destroyBuffer(get());
}

 Buffer& Buffer::operator=(Buffer&& dev) {
  swap(dev);
  return *this;
 }

 void Buffer::swap(Buffer& dev) {
  // std::swap(m_device, dev.m_device);
  std::swap(get(), dev.get());
  std::swap(m_memory, dev.m_memory);
  std::swap(m_mem_info, dev.m_mem_info);
  std::swap(m_device, dev.m_device);
 }