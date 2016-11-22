#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "wrapper.hpp"

template<typename T, typename U>
class Resource;
#include <vulkan/vulkan.hpp>

class Buffer;
class Image;
class Device;

uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties);

using WrapperMemory = Wrapper<vk::DeviceMemory, vk::MemoryAllocateInfo>;
class Memory : public WrapperMemory {
 public:
  
  Memory();
  Memory(Device const& dev, uint32_t type_index, vk::DeviceSize const& size);
  Memory(Device const& dev, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Device const& device, void* data, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& memProperties);
  Memory(Memory && dev);
  Memory(Memory const&) = delete;
  ~Memory();

  Memory& operator=(Memory const&) = delete;
  Memory& operator=(Memory&& dev);

  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);
  vk::DeviceSize bindResource(Buffer const& buffer);
  vk::DeviceSize bindResource(Image const& buffer);
  // vk::DeviceSize bindBuffer(Buffer const& buffer);
  // vk::DeviceSize bindImage(Image const& buffer);
  // overwrite/alias memory range
  // vk::DeviceSize bindBuffer(Buffer const& buffer, vk::DeviceSize offset);
  // vk::DeviceSize bindImage(Image const& buffer, vk::DeviceSize offset);
  vk::DeviceSize bindResource(Buffer const& resource, vk::DeviceSize offset);
  vk::DeviceSize bindResource(Image const& resource, vk::DeviceSize offset);
  template<typename T,typename U>
  vk::DeviceSize bindResource(Resource<T, U> const& resource);
  template<typename T,typename U>
  vk::DeviceSize bindResource(Resource<T, U> const& resource, vk::DeviceSize offset);
  vk::DeviceSize size() const;
  vk::DeviceSize space() const;
  uint32_t memoryType() const;
  
  void swap(Memory& dev);
  
 private:
  void bindResourceMemory(Buffer const& buffer, vk::DeviceSize offset);
  void bindResourceMemory(Image const& image, vk::DeviceSize offset);

  void destroy() override;

  Device const* m_device;
  vk::DeviceSize m_offset;
};

#endif