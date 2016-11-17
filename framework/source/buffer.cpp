#include "buffer.hpp"

#include "device.hpp"

#include <iostream>

Buffer::Buffer()
 :WrapperBuffer{}
 ,m_memory{}
 // ,m_mem_info{}
 ,m_device{nullptr}
 ,m_desc_info{}
{}

Buffer::Buffer(Buffer && buffer)
 :Buffer{}
{
  swap(buffer);
}

Buffer::Buffer(Device const& device, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties)
 :Buffer{}
{
  m_device = &device;

  m_info.size = size;
  m_info.usage = usage;
  auto queueFamilies = device.ownerIndices();
  if (queueFamilies.size() > 1) {
    m_info.sharingMode = vk::SharingMode::eConcurrent;
    m_info.queueFamilyIndexCount = std::uint32_t(queueFamilies.size());
    m_info.pQueueFamilyIndices = queueFamilies.data();
  }
  else {
    m_info.sharingMode = vk::SharingMode::eExclusive;
  }
  m_object =device->createBuffer(info());

  auto memRequirements = device->getBufferMemoryRequirements(get());
  m_memory = Memory{device, memRequirements, memProperties};

  m_memory.bindBuffer(*this, 0);

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

void Buffer::setData(void const* data, vk::DeviceSize const& size) {
  m_memory.setData(data, size);
}

void Buffer::destroy() {
  (*m_device)->destroyBuffer(get());
}

 Buffer& Buffer::operator=(Buffer&& buffer) {
  swap(buffer);
  return *this;
 }

vk::DescriptorBufferInfo const& Buffer::descriptorInfo() const {
  return m_desc_info;
}
// todo: write descriptor set wrapper to allow automatic type detection
void Buffer::writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  if (m_info.usage & vk::BufferUsageFlagBits::eUniformBuffer) {
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
  }
  else if (m_info.usage & vk::BufferUsageFlagBits::eStorageBuffer) {
    descriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
  }
  else {
    throw std::runtime_error{"buffer usage '" + to_string(m_info.usage) + "' not supported for descriptor write"};
  }
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &m_desc_info;
  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

 void Buffer::swap(Buffer& buffer) {
  WrapperBuffer::swap(buffer);
  std::swap(m_memory, buffer.m_memory);
  std::swap(m_device, buffer.m_device);
  std::swap(m_desc_info, buffer.m_desc_info);
 }