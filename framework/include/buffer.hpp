#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrapper.hpp"
#include "memory.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;
class Device;

using WrapperBuffer = Wrapper<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public WrapperBuffer {
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties);
  Buffer(Device const& device, void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  ~Buffer();
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void setData(void const* data, vk::DeviceSize const& size);
  void bindTo(Memory& memory);

  void swap(Buffer& dev);
  vk::DeviceSize size() const;
  vk::MemoryRequirements requirements() const;
  vk::MemoryPropertyFlags const& memFlags() const;
  vk::DescriptorBufferInfo const& descriptorInfo() const;
  uint32_t memoryType() const;

  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const;

 private:
  void destroy() override;

  vk::DeviceMemory m_memory;
  Device const* m_device;
  vk::DescriptorBufferInfo m_desc_info;
  vk::DeviceSize m_offset;
  vk::MemoryPropertyFlags m_flags_mem;
};

#endif