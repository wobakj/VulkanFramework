#ifndef BUFFER_VIEW_HPP
#define BUFFER_VIEW_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/buffer.hpp"

#include <vulkan/vulkan.hpp>

// subresources which can be bound to a buffer and adescriptor set
class BufferView : public BufferRegion {
 public:
  
  BufferView();
  BufferView(vk::DeviceSize const& size, vk::BufferUsageFlagBits const& usage);
  BufferView(BufferView && dev);
  BufferView(BufferView const&) = delete;
  
  BufferView& operator=(BufferView const&) = delete;
  BufferView& operator=(BufferView&& dev);

  virtual void bindTo(Buffer& buffer);
  virtual void bindTo(Buffer& buffer, vk::DeviceSize const& offset);
  
 private:
  void swap(BufferView& dev);

  vk::Device m_device;
  vk::BufferUsageFlagBits m_usage;
};

#endif