#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

// a buffer subresource
class BufferRegion {
 public:
  BufferRegion();
  BufferRegion(vk::Buffer const& buffer, vk::DeviceSize size, vk::DeviceSize offset = 0);

  vk::Buffer const& buffer() const;
  vk::DeviceSize size() const;
  vk::DeviceSize offset() const;

 protected:
  void swap(BufferRegion& dev);

  vk::DescriptorBufferInfo m_info;
};

class SwapChain;
class Device;
class Memory;
class BufferView;
class BufferRegion;

using ResourceBuffer = MemoryResourceT<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public ResourceBuffer{
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  ~Buffer();
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void swap(Buffer& dev);
  vk::MemoryRequirements requirements() const override;

  vk::DeviceSize bindOffset(BufferView const&);
  vk::DeviceSize bindOffset(BufferView const&, vk::DeviceSize offset);
  vk::DeviceSize space() const;
  vk::DeviceSize size() const;

  void bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset) override;
  void writeToSet(vk::DescriptorSet const& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;
  virtual res_handle_t handle() const override {
    return res_handle_t{m_object};
  }

  operator BufferRegion const() const; 

 private:
  void destroy() override;

  vk::DeviceSize m_offset_view;
  friend class BufferView;
};

#endif