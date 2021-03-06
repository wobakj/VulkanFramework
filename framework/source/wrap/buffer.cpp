#include "wrap/buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/memory.hpp"
#include "wrap/buffer_view.hpp"

#include <iostream>
#include <cmath>

BufferRegion::BufferRegion()
 :m_info{}
{}

BufferRegion::BufferRegion(vk::Buffer const& buffer, vk::DeviceSize size, vk::DeviceSize offset)
 :m_info{buffer, offset, size}
{}

vk::DeviceSize BufferRegion::size() const {
  return m_info.range;
}

vk::DeviceSize BufferRegion::offset() const {
  return m_info.offset;
}

vk::Buffer const& BufferRegion::buffer() const {
  return m_info.buffer;
}

void BufferRegion::swap(BufferRegion& rhs) {
  std::swap(m_info, rhs.m_info);
 }

BufferRegion::operator vk::DescriptorBufferInfo const&() const {
  return m_info;
}

///////////////////////////////////////////////////////////////////////////////

Buffer::Buffer()
 :ResourceBuffer{}
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
  // fulfill alignment requirements of object
  auto alignmnt = alignment();
  offset = alignmnt * vk::DeviceSize(std::ceil(float(offset) / float(alignmnt)));
  // store new offset
  m_offset_view = std::max(m_offset_view, offset + view.size());
  return offset;
}

vk::DeviceSize Buffer::space() const {
  return size() - m_offset_view;
}


vk::DeviceSize Buffer::size() const {
  return info().size;
}

void Buffer::bindTo(vk::DeviceMemory const& mem, vk::DeviceSize const& offst) {
  ResourceBuffer::bindTo(mem, offst);
  m_device.bindBufferMemory(get(), mem, offst);
}

Buffer::operator BufferRegion const() const {
  return BufferRegion{get(), size(), 0};
}
