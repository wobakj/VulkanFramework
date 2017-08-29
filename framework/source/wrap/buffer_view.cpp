#include "wrap/buffer_view.hpp"

#include "wrap/buffer.hpp"
#include "wrap/device.hpp"

#include <iostream>

BufferSubresource::BufferSubresource()
 :m_info{}
{}

BufferSubresource::BufferSubresource(vk::Buffer const& buffer, vk::DeviceSize size, vk::DeviceSize offset)
 :m_info{buffer, offset, size}
{}

vk::DeviceSize BufferSubresource::size() const {
  return m_info.range;
}

vk::DeviceSize BufferSubresource::offset() const {
  return m_info.offset;
}

vk::Buffer const& BufferSubresource::buffer() const {
  return m_info.buffer;
}

void BufferSubresource::swap(BufferSubresource& rhs) {
  std::swap(m_info, rhs.m_info);
 }


BufferView::BufferView()
 :BufferSubresource{}
 ,m_device{}
{}

BufferView::BufferView(BufferView && rhs)
 :BufferView{}
{
  swap(rhs);
}

BufferView::BufferView(vk::DeviceSize const& size, vk::BufferUsageFlagBits const& usage)
 :BufferView{}
{
  m_info.range = size;
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
  descriptorWrite.pBufferInfo = &m_info;
  m_device.updateDescriptorSets({descriptorWrite}, 0);
}

void BufferView::swap(BufferView& rhs) {
  BufferSubresource::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_usage, rhs.m_usage);
 }

void BufferView::bindTo(Buffer& buffer) {
  m_info.buffer = buffer.get();
  m_info.offset = buffer.bindOffset(*this);
  m_device = buffer.m_device;
}

void BufferView::bindTo(Buffer& buffer, vk::DeviceSize const& offset) {
  m_info.buffer = buffer.get();
  m_info.offset = buffer.bindOffset(*this, offset);
  m_device = buffer.m_device;
}
