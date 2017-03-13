#ifndef BUFFER_VIEW_HPP
#define BUFFER_VIEW_HPP

#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class Buffer;
// class to divide buffer into subresources which can be bound to a drescriptor set
class BufferView : public MappableResource {
 public:
  
  BufferView();
  BufferView(vk::DeviceSize const& size, vk::BufferUsageFlagBits const& usage);
  BufferView(BufferView && dev);
  BufferView(BufferView const&) = delete;
  
  BufferView& operator=(BufferView const&) = delete;
  BufferView& operator=(BufferView&& dev);

  virtual void bindTo(Buffer& buffer);
  virtual void bindTo(Buffer& buffer, vk::DeviceSize const& offset);

  vk::DeviceSize size() const;
  vk::DeviceSize offset() const;
  vk::Buffer const& buffer() const;
  
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;

 private:
  void swap(BufferView& dev);

  vk::DescriptorBufferInfo m_desc_info;
  vk::BufferUsageFlagBits m_usage;
};

#endif