#include "wrap/buffer_view.hpp"

#include "wrap/buffer.hpp"
#include "wrap/device.hpp"

#include <iostream>


BufferView::BufferView()
 :BufferRegion{}
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

void BufferView::swap(BufferView& rhs) {
  BufferRegion::swap(rhs);
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
