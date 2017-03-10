#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;
class Device;
class Memory;
class BufferView;

using ResourceBuffer = MemoryResourceT<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public ResourceBuffer {
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

  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const;
  
  vk::DeviceSize bindView(BufferView const&);
  vk::DeviceSize bindView(BufferView const&, vk::DeviceSize offset);
  vk::DeviceSize space() const;

  void bindTo(Memory& memory);
  void bindTo(Memory& memory, vk::DeviceSize const& offset);

  virtual res_handle_t handle() const override {
    return res_handle_t{m_object};
  }
 private:
  void destroy() override;

  vk::DescriptorBufferInfo m_desc_info;
  vk::DeviceSize m_offset_view;
  friend class BufferView;
};

#endif