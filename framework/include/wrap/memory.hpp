#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;

using WrapperMemory = Wrapper<vk::DeviceMemory, vk::MemoryAllocateInfo>;
class Memory : public WrapperMemory {
 public:
  
  Memory();
  Memory(Device const& dev, uint32_t type_index, vk::DeviceSize const& size);
  Memory(Device const& device, uint32_t type_bits, vk::MemoryPropertyFlags const& properties, vk::DeviceSize const& size);
  Memory(Device const& dev, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Device const& device, void* data, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Memory && dev);
  Memory(Memory const&) = delete;
  ~Memory();

  Memory& operator=(Memory const&) = delete;
  Memory& operator=(Memory&& dev);

  void* map(vk::DeviceSize const& size, vk::DeviceSize const& offset);
  void unmap();
  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);

  vk::DeviceSize bindOffset(vk::MemoryRequirements const& requirements);
  vk::DeviceSize bindOffset(vk::MemoryRequirements const& requirements, vk::DeviceSize offset);
  
  vk::DeviceSize size() const;
  vk::DeviceSize space() const;
  uint32_t memoryType() const;
  
  void swap(Memory& dev);
  
 private:
  void destroy() override;

  Device const* m_device;
  vk::DeviceSize m_offset;
};

#include "wrap/memory_resource.hpp"

#endif