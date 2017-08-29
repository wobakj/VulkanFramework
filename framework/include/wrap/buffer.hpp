#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

// interface to represent a buffer subresource
class BufferRegion {
 public:
  virtual vk::Buffer const& buffer() const = 0;
  virtual vk::DeviceSize size() const = 0;
  virtual vk::DeviceSize offset() const = 0;
};

class SwapChain;
class Device;
class Memory;
class BufferView;

using ResourceBuffer = MemoryResourceT<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public ResourceBuffer, public BufferRegion {
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

  void bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset) override;
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;
  virtual res_handle_t handle() const override {
    return res_handle_t{m_object};
  }

  virtual vk::Buffer const& buffer() const override;
  virtual vk::DeviceSize size() const override;
  virtual vk::DeviceSize offset() const override;

 private:
  void destroy() override;

  vk::DeviceSize m_offset_view;
  friend class BufferView;
};

#endif