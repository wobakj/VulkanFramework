#include "wrap/memory.hpp"

#include "wrap/device.hpp"
#include "wrap/buffer.hpp"
#include "wrap/image.hpp"

#include <iostream>
#include <cmath>

// bool index_matches_filter(uint32_t index, uint32_t type_filter) {
//   return (type_filter & (1u << index)) != (1u << index);
// }

uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
  auto memProperties = device.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (index_matches_filter(i, typeFilter) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
  return 0;
}

Memory::Memory()
 :WrapperMemory{}
 ,m_device{nullptr}
 ,m_offset{0}
{}

Memory::Memory(Memory && mem)
 :Memory{}
{
  swap(mem);
}

Memory::Memory(Device const& device, uint32_t type_index, vk::DeviceSize const& size)
 :Memory{}
{
  m_device = &device;
  m_info.allocationSize = size;
  m_info.memoryTypeIndex = type_index;
  m_object = device->allocateMemory(info());
}

Memory::Memory(Device const& device, uint32_t type_bits, vk::MemoryPropertyFlags const& properties, vk::DeviceSize const& size)
 :Memory{device, findMemoryType(device.physical(), type_bits, properties), size}
{}

Memory::Memory(Device const& device, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& properties)
 :Memory{device, findMemoryType(device.physical(), requirements.memoryTypeBits, properties), requirements.size}
{}

Memory::Memory(Device const& device, void* data, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& properties) 
 :Memory{device, requirements, properties}
{
  setData(data, requirements.size);
}

Memory::~Memory() {
  cleanup();
}

void* Memory::map(vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  return (*m_device)->mapMemory(get(), offset, size);
}

void Memory::unmap() {
  (*m_device)->unmapMemory(get());
}

void Memory::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  void* ptr = map(size, offset);
  std::memcpy(ptr, data, size_t(size));
  unmap();
}

void Memory::destroy() {
  (*m_device)->freeMemory(get());
}

 Memory& Memory::operator=(Memory&& dev) {
  swap(dev);
  return *this;
 }

 void Memory::swap(Memory& mem) {
  WrapperMemory::swap(mem);
  std::swap(m_device, mem.m_device);
  std::swap(m_offset, mem.m_offset);
 }

vk::DeviceSize Memory::size() const {
  return info().allocationSize;
}
vk::DeviceSize Memory::space() const {
  return size() - m_offset;
}

uint32_t Memory::memoryType() const {
  return m_info.memoryTypeIndex;
}

vk::DeviceSize Memory::bindOffset(vk::MemoryRequirements const& requirements) {
  return bindOffset(requirements, m_offset);
}

vk::DeviceSize Memory::bindOffset(vk::MemoryRequirements const& requirements, vk::DeviceSize offset) {
  if (offset + requirements.size > size()) {
    throw std::out_of_range{"Resource size " + std::to_string(requirements.size) + " with offset " + std::to_string(offset) + " too large for free memory " + std::to_string(space()) + " from " + std::to_string(size())};
  }
  // fulfill allignment requirements of object
  auto alignment = requirements.alignment;
  offset = alignment * vk::DeviceSize(std::ceil(float(offset) / float(alignment)));
  // bindOffsetMemory(requirements.get, offset);
  // store new offset
  m_offset = std::max(m_offset, offset + requirements.size);
  return offset;
}
