#include "wrap/buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/memory.hpp"
#include "wrap/buffer_view.hpp"

#include <iostream>

Buffer::Buffer()
 :ResourceBuffer{}
 ,m_desc_info{}
 ,m_offset_view{0}
{}

Buffer::Buffer(Buffer && buffer)
 :Buffer{}
{
  swap(buffer);
}

Buffer::Buffer(Device const& device, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage)
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
  m_object = device->createBuffer(info());

  m_desc_info.buffer = get();
  m_desc_info.offset = 0;
  m_desc_info.range = size;
}

Buffer::~Buffer() {
  cleanup();
}

void Buffer::destroy() {
  (*m_device)->destroyBuffer(get());
}

 Buffer& Buffer::operator=(Buffer&& buffer) {
  swap(buffer);
  return *this;
 }

// todo: write descriptor set wrapper to allow automatic type detection
void Buffer::writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
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

vk::MemoryRequirements Buffer::requirements() const {
  return (*m_device)->getBufferMemoryRequirements(get());
}

void Buffer::swap(Buffer& buffer) {
  ResourceBuffer::swap(buffer);
  std::swap(m_desc_info, buffer.m_desc_info);
  std::swap(m_offset_view, buffer.m_offset_view);
 }

void Buffer::setMemory(Memory& memory) {
  m_memory = &memory;
}

vk::DeviceSize Buffer::bindView(BufferView const& view) {
  return bindView(view, m_offset_view);
}

vk::DeviceSize Buffer::bindView(BufferView const& view, vk::DeviceSize offset) {
  if (offset + view.size() > size()) {
    throw std::out_of_range{"View size " + std::to_string(view.size()) + " too large for buffer " + std::to_string(space()) + " from " + std::to_string(size())};
  }
  // fulfill allignment requirements of object
  auto alignmnt = alignment();
  offset = alignmnt * vk::DeviceSize(std::ceil(float(offset) / float(alignmnt)));
  // store new offset
  m_offset_view = std::max(m_offset_view, offset + view.size());
  return offset;
}

vk::DeviceSize Buffer::space() const {
  return size() - m_offset_view;
}

void* Buffer::map() {
  return map(size(), m_offset);
}

void* Buffer::map(vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  return m_memory->map(size, m_offset + offset);
}

void Buffer::unmap() {
  m_memory->unmap();
}