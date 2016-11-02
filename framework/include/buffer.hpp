#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrapper.hpp"
#include "device.hpp"
#include "memory.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;

using WrapperBuffer = Wrapper<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public WrapperBuffer {
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties);
  Buffer(Device const& device, void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void setData(void const* data, vk::DeviceSize const& size);

  void swap(Buffer& dev);
  
  vk::DescriptorBufferInfo const& descriptorInfo() const;
  void writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const;

 private:
  void destroy() override;

  Memory m_memory;
  // vk::DeviceMemory m_memory;
  // vk::MemoryAllocateInfo m_mem_info;
  Device const* m_device;
  vk::DescriptorBufferInfo m_desc_info;
};

#endif