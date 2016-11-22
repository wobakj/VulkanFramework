#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrapper.hpp"
#include "resource.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;
class Device;
class Memory;

using WrapperBuffer = Wrapper<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public WrapperBuffer {
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  ~Buffer();
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);
  void bindTo(Memory& memory);
  void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void swap(Buffer& dev);
  vk::DeviceSize size() const;
  vk::MemoryRequirements requirements() const;
  // vk::DescriptorBufferInfo const& descriptorInfo() const;
  uint32_t memoryTypeBits() const;

  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const;

 private:
  void destroy() override;

  Device const* m_device;
  Memory* m_memory;
  vk::DescriptorBufferInfo m_desc_info;
  vk::DeviceSize m_offset;
};

#endif