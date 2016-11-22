#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include "wrapper.hpp"
// #include "device.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

template<typename T, typename U>
class Resource : public Wrapper<T, U> {
  using WrapperResource = Wrapper<T, U>;
 public:
  Resource()
   :WrapperResource{}
   ,m_device{nullptr}
   ,m_memory{nullptr}
  {}

  void bindTo(Memory& memory);

  void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);

  // virtual void bindTo(Memory& memory);
  // virtual void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void swap(Resource& rhs) {
    WrapperResource::swap(rhs);
    std::swap(m_memory, rhs.m_memory);
    std::swap(m_device, rhs.m_device);
    std::swap(m_offset, rhs.m_offset);
  }

  vk::DeviceSize size() const {
    return requirements().size;
  }

  vk::DeviceSize alignment() const {
    return requirements().alignment;
  }

  virtual vk::MemoryRequirements requirements() const = 0;

  uint32_t memoryTypeBits() const {
    return requirements().memoryTypeBits;
  }

  virtual void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const = 0;

 protected:
  Device const* m_device;
  Memory* m_memory;
  vk::DeviceSize m_offset;
};

#include "memory.hpp"

template<typename T, typename U>
  void Resource<T, U>::bindTo(Memory& memory) {
    m_offset = memory.bindResource(*this);
    m_memory = &memory;
  }
template<typename T, typename U>
  void Resource<T, U>::bindTo(Memory& memory, vk::DeviceSize const& offset) {
    m_offset = memory.bindResource(*this, offset);
    m_memory = &memory;
  }
template<typename T, typename U>
  void Resource<T, U>::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
    m_memory->setData(data, size, m_offset + offset);
  }


#endif