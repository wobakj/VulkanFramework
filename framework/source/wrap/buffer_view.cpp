#include "wrap/buffer_view.hpp"

#include "wrap/buffer.hpp"
#include "wrap/device.hpp"

#include <iostream>

BufferView::BufferView()
 :m_desc_info{}
{}

BufferView::BufferView(BufferView && rhs)
 :BufferView{}
{
  swap(rhs);
}

BufferView::BufferView(vk::DeviceSize const& size, vk::BufferUsageFlagBits const& usage)
 :BufferView{}
{
  m_desc_info.range = size;
  m_usage = usage;
}

BufferView& BufferView::operator=(BufferView&& rhs) {
  swap(rhs);
  return *this;
}

// todo: write descriptor set wrapper to allow automatic type detection
void BufferView::writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &m_desc_info;
  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

void BufferView::swap(BufferView& rhs) {
  MappableResource::swap(rhs);
  std::swap(m_desc_info, rhs.m_desc_info);
  std::swap(m_usage, rhs.m_usage);
 }

void BufferView::bindTo(Buffer& buffer) {
  m_desc_info.buffer = buffer.get();
  m_desc_info.offset = buffer.bindOffset(*this);
  MappableResource::bindTo(buffer.memory(), buffer.offset() + m_desc_info.offset);
  m_device = buffer.m_device;
}

void BufferView::bindTo(Buffer& buffer, vk::DeviceSize const& offset) {
  m_desc_info.buffer = buffer.get();
  m_desc_info.offset = buffer.bindOffset(*this, offset);
  MappableResource::bindTo(buffer.memory(), buffer.offset() + m_desc_info.offset);
  m_device = buffer.m_device;
}

vk::DeviceSize BufferView::size() const {
  return m_desc_info.range;
}

vk::DeviceSize BufferView::offset() const {
  return m_desc_info.offset;
}

vk::Buffer const& BufferView::buffer() const {
  return m_desc_info.buffer;
}
