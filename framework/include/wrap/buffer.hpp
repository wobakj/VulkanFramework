#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;
class Device;
class Memory;
class BufferView;

using ResourceBuffer = MemoryResource<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public ResourceBuffer {
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  ~Buffer();
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void setMemory(Memory& memory);

  void swap(Buffer& dev);
  vk::MemoryRequirements requirements() const;

  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const;
  
  vk::DeviceSize bindView(BufferView const&);
  vk::DeviceSize bindView(BufferView const&, vk::DeviceSize offset);
  vk::DeviceSize space() const;

  void* map(vk::DeviceSize const& size, vk::DeviceSize const& offset);
  void* map();
  void unmap();

  void bindTo(Memory& memory);
  void bindTo(Memory& memory, vk::DeviceSize const& offset);

 private:
  void destroy() override;

  vk::DescriptorBufferInfo m_desc_info;
  vk::DeviceSize m_offset_view;
  friend class BufferView;
};

#endif