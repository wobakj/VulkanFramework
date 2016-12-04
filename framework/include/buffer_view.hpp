#ifndef BUFFER_VIEW_HPP
#define BUFFER_VIEW_HPP

#include "memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class Buffer;

class BufferView {
 public:
  
  BufferView();
  BufferView(vk::DeviceSize const& size);
  BufferView(BufferView && dev);
  BufferView(BufferView const&) = delete;
  
  BufferView& operator=(BufferView const&) = delete;
  BufferView& operator=(BufferView&& dev);

  virtual void bindTo(Buffer& buffer);
  virtual void bindTo(Buffer& buffer, vk::DeviceSize const& offset);

  void setData(void const* data);
  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);

  vk::DeviceSize size() const;
  vk::DeviceSize offset() const;
  Buffer& buffer() const;
   
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const;

 private:
  void swap(BufferView& dev);

  Buffer* m_buffer;
  vk::DescriptorBufferInfo m_desc_info;
};

#endif