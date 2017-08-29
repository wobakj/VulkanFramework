#include "wrap/buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/memory.hpp"
#include "wrap/buffer_view.hpp"

#include <iostream>
#include <cmath>

Buffer::Buffer()
 :ResourceBuffer{}
 ,BufferRegion{}
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
  m_device = device.get();

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
}

Buffer::~Buffer() {
  cleanup();
}

void Buffer::destroy() {
  m_device.destroyBuffer(get());
}

Buffer& Buffer::operator=(Buffer&& buffer) {
  swap(buffer);
  return *this;
}

vk::MemoryRequirements Buffer::requirements() const {
  return m_device.getBufferMemoryRequirements(get());
}

void Buffer::swap(Buffer& buffer) {
  ResourceBuffer::swap(buffer);
  // BufferRegion::swap(buffer);
  std::swap(m_offset_view, buffer.m_offset_view);
 }

vk::DeviceSize Buffer::bindOffset(BufferView const& view) {
  return bindOffset(view, m_offset_view);
}

vk::DeviceSize Buffer::bindOffset(BufferView const& view, vk::DeviceSize offset) {
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

void Buffer::bindTo(vk::DeviceMemory const& mem, vk::DeviceSize const& offst) {
  ResourceBuffer::bindTo(mem, offst);
  m_device.bindBufferMemory(get(), mem, offst);
}

void Buffer::writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index) const {
  vk::DescriptorBufferInfo info_buffer{get(), 0, size()};

  vk::WriteDescriptorSet info_write{};
  info_write.dstSet = set;
  info_write.dstBinding = binding;
  info_write.dstArrayElement = index;
  info_write.descriptorType = type;
  info_write.descriptorCount = 1;
  info_write.pBufferInfo = &info_buffer;
  m_device.updateDescriptorSets({info_write}, 0);
}

vk::DeviceSize Buffer::size() const {
  return info().size;
}

vk::DeviceSize Buffer::offset() const {
  return 0;
}

vk::Buffer const& Buffer::buffer() const {
  return get();
}
