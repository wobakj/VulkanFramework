#include "buffer_view.hpp"

#include "buffer.hpp"
#include "device.hpp"

#include <iostream>

BufferView::BufferView()
 :m_buffer{nullptr}
 ,m_desc_info{}
{}

BufferView::BufferView(BufferView && bufferView)
 :BufferView{}
{
  swap(bufferView);
}

BufferView::BufferView(vk::DeviceSize const& size)
 :BufferView{}
{
  m_desc_info.range = size;
}

 BufferView& BufferView::operator=(BufferView&& bufferView) {
  swap(bufferView);
  return *this;
 }

// todo: write descriptor set wrapper to allow automatic type detection
void BufferView::writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  if (m_buffer->info().usage & vk::BufferUsageFlagBits::eUniformBuffer) {
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
  }
  else if (m_buffer->info().usage & vk::BufferUsageFlagBits::eStorageBuffer) {
    descriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
  }
  else {
    throw std::runtime_error{"bufferView usage '" + to_string(m_buffer->info().usage) + "' not supported for descriptor write"};
  }
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &m_desc_info;
  m_buffer->m_device->get().updateDescriptorSets({descriptorWrite}, 0);
}

void BufferView::swap(BufferView& bufferView) {
  std::swap(m_buffer, bufferView.m_buffer);
  std::swap(m_desc_info, bufferView.m_desc_info);
 }

void BufferView::bindTo(Buffer& buffer) {
  m_desc_info.offset = buffer.bindView(*this);
  m_desc_info.buffer = buffer.get();
  m_buffer = &buffer;
}

void BufferView::bindTo(Buffer& buffer, vk::DeviceSize const& offset) {
  m_desc_info.offset = buffer.bindView(*this, offset);
  m_desc_info.buffer = buffer.get();
  m_buffer = &buffer;
}

void BufferView::setBuffer(Buffer& buffer) {
  m_desc_info.buffer = buffer.get();
  m_buffer = &buffer;  
}

void BufferView::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  m_buffer->setData(data, m_desc_info.range, m_desc_info.offset + offset);
}

void BufferView::setData(void const* data) {
  setData(data, m_desc_info.range, 0);
}

vk::DeviceSize BufferView::size() const {
  return m_desc_info.range;
}

vk::DeviceSize BufferView::offset() const {
  return m_desc_info.offset;
}

Buffer& BufferView::buffer() const {
  return *m_buffer;
}

