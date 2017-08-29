#ifndef BUFFER_VIEW_HPP
#define BUFFER_VIEW_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/buffer.hpp"

#include <vulkan/vulkan.hpp>

// actual instance of a subresource
class BufferSubresource : public BufferRegion {
 public:
  BufferSubresource();
  BufferSubresource(vk::Buffer const& buffer, vk::DeviceSize size, vk::DeviceSize offset = 0);

  virtual vk::Buffer const& buffer() const override;
  virtual vk::DeviceSize size() const override;
  virtual vk::DeviceSize offset() const override;

 protected:
  void swap(BufferSubresource& dev);

  vk::DescriptorBufferInfo m_info;
};

// subresources which can be bound to a buffer and adescriptor set
class BufferView : public BufferSubresource {
 public:
  
  BufferView();
  BufferView(vk::DeviceSize const& size, vk::BufferUsageFlagBits const& usage);
  BufferView(BufferView && dev);
  BufferView(BufferView const&) = delete;
  
  BufferView& operator=(BufferView const&) = delete;
  BufferView& operator=(BufferView&& dev);

  virtual void bindTo(Buffer& buffer);
  virtual void bindTo(Buffer& buffer, vk::DeviceSize const& offset);
  
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;

 private:
  void swap(BufferView& dev);

  vk::Device m_device;
  vk::BufferUsageFlagBits m_usage;
};

#endif