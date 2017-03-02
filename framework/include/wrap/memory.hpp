#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "wrap/wrapper.hpp"

template<typename T, typename U>
class MemoryResource;

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

  template<typename T,typename U>
  vk::DeviceSize bindResource(MemoryResource<T, U> const& resource);
  template<typename T,typename U>
  vk::DeviceSize bindResource(MemoryResource<T, U> const& resource, vk::DeviceSize offset);
  
  vk::DeviceSize size() const;
  vk::DeviceSize space() const;
  uint32_t memoryType() const;
  
  void swap(Memory& dev);
  
 private:
  void bindResourceMemory(vk::Buffer const& buffer, vk::DeviceSize offset);
  void bindResourceMemory(vk::Image const& image, vk::DeviceSize offset);

  void destroy() override;

  Device const* m_device;
  vk::DeviceSize m_offset;
};


#include "wrap/memory_resource.hpp"
#include <cmath>

template<typename T, typename U>
vk::DeviceSize Memory::bindResource(MemoryResource<T, U> const& image) {
  return bindResource<T, U>(image, m_offset);
}

template<typename T, typename U>
vk::DeviceSize Memory::bindResource(MemoryResource<T, U> const& resource, vk::DeviceSize offset) {
  if (offset + resource.size() > size()) {
    throw std::out_of_range{"Resource size " + std::to_string(resource.size()) + " with offset " + std::to_string(offset) + " too large for free memory " + std::to_string(space()) + " from " + std::to_string(size())};
  }
  // fulfill allignment requirements of object
  auto alignment = resource.alignment();
  offset = alignment * vk::DeviceSize(std::ceil(float(offset) / float(alignment)));
  bindResourceMemory(resource.get(), offset);
  // store new offset
  m_offset = std::max(m_offset, offset + resource.size());
  return offset;
}

#endif