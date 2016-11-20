#include "memory.hpp"

#include "device.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "resource.hpp"

#include <iostream>
#include <cmath>

uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
  auto memProperties = device.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
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

Memory::Memory(Device const& device, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& properties)
 :Memory{device, findMemoryType(device.physical(), requirements.memoryTypeBits, properties), requirements.size}
{
  std::cout << "use memory type " << m_info.memoryTypeIndex << " for " << requirements.memoryTypeBits << " and " << to_string(properties) << std::endl;
}

Memory::Memory(Device const& device, void* data, vk::MemoryRequirements const& requirements, vk::MemoryPropertyFlags const& properties) 
 :Memory{device, requirements, properties}
{
  setData(data, requirements.size);
}

Memory::~Memory() {
  cleanup();
}

void Memory::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  // assert(offset +)
  void* buff_ptr = (*m_device)->mapMemory(get(), offset, size);
  std::memcpy(buff_ptr, data, size_t(size));
  (*m_device)->unmapMemory(get());
}

void Memory::bindResourceMemory(vk::Buffer const& buffer, vk::DeviceSize offset) {
  (*m_device)->bindBufferMemory(buffer, get(), offset);
}

void Memory::bindResourceMemory(vk::Image const& image, vk::DeviceSize offset) {
  (*m_device)->bindImageMemory(image, get(), offset);
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
