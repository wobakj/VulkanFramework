#include "buffer.hpp"

#include <iostream>
#include "device.hpp"


uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
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
 ,m_desc_info{}
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

  info().size = size;
  info().usage = usage;
  info().sharingMode = vk::SharingMode::eExclusive;
  get() = device->createBuffer(info());

  auto memRequirements = device->getBufferMemoryRequirements(get());

  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(device.physical(), memRequirements.memoryTypeBits, memProperties);
  m_memory = device->allocateMemory(allocInfo);

  device->bindBufferMemory(get(), m_memory, 0);

  m_desc_info.buffer = get();
  m_desc_info.offset = 0;
  m_desc_info.range = size;
}

Buffer::Buffer(Device const& device, void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) 
 :Buffer{}
{
  m_device = &device;
  // create staging buffer and upload data
  Buffer buffer_stage{device, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  
  buffer_stage.setData(data, size);
  // create storage buffer and transfer data
  Buffer buffer_store{device, size, usage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal};

  device.copyBuffer(buffer_stage.get(), buffer_store.get(), size);
  // assign filled buffer to this
  swap(buffer_store);
}

void Buffer::setData(void* data, vk::DeviceSize const& size) {
  void* buff_ptr = (*m_device)->mapMemory(m_memory, 0, size);
  std::memcpy(buff_ptr, data, size_t(size));
  (*m_device)->unmapMemory(m_memory);
}

void Buffer::destroy() {
  (*m_device)->freeMemory(m_memory);
  (*m_device)->destroyBuffer(get());
}

 Buffer& Buffer::operator=(Buffer&& dev) {
  swap(dev);
  return *this;
 }

vk::DescriptorBufferInfo const& Buffer::descriptorInfo() const {
  return m_desc_info;
}

void Buffer::writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &m_desc_info;
  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

 void Buffer::swap(Buffer& dev) {
  WrapperBuffer::swap(dev);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_mem_info, dev.m_mem_info);
  std::swap(m_device, dev.m_device);
  std::swap(m_desc_info, dev.m_desc_info);
 }