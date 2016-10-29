#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrapper.hpp"
#include "device.hpp"

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

  void swap(Buffer& dev);
  
 private:
  void destroy() override;

  vk::DeviceMemory m_memory;
  vk::MemoryAllocateInfo m_mem_info;
  Device const* m_device;
};

#endif