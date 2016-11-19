#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Buffer;
class Image;
class Device;

uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties);

using WrapperMemory = Wrapper<vk::DeviceMemory, vk::MemoryAllocateInfo>;
class Memory : public WrapperMemory {
 public:
  
  Memory();
  Memory(Device const& dev, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Device const& device, void* data, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Memory && dev);
  Memory(Memory const&) = delete;
  ~Memory();

  Memory& operator=(Memory const&) = delete;
  Memory& operator=(Memory&& dev);

  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);
  vk::DeviceSize bindBuffer(Buffer const& buffer);
  vk::DeviceSize bindImage(Image const& buffer);
  vk::DeviceSize size() const;
  vk::DeviceSize space() const;
  
  void swap(Memory& dev);
  
 private:
  void destroy() override;

  Device const* m_device;
  vk::DeviceSize m_offset;
};

#endif