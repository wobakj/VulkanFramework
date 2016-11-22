#include "resource.hpp"

#include "device.hpp"
#include "memory.hpp"

#include <iostream>

Recource::Recource()
 :m_device{nullptr}
 ,m_memory{nullptr}
 ,m_offset{0}
{}

void Recource::bindTo(Memory& memory) {
  m_offset = memory.bindRecource(*this);
  m_memory = &memory;
}

void Recource::bindTo(Memory& memory, vk::DeviceSize const& offset) {
  m_offset = memory.bindRecource(*this, offset);
  m_memory = &memory;
}

void Recource::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  m_memory->setData(data, size, m_offset + offset);
}

void Recource::destroy() {
  (*m_device)->destroyRecource(get());
}

 Recource& Recource::operator=(Recource&& buffer) {
  swap(buffer);
  return *this;
 }

// vk::DescriptorRecourceInfo const& Recource::descriptorInfo() const {
//   return m_desc_info;
// }
// todo: write descriptor set wrapper to allow automatic type detection
void Recource::writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  if (m_info.usage & vk::RecourceUsageFlagBits::eUniformRecource) {
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformRecource;
  }
  else if (m_info.usage & vk::RecourceUsageFlagBits::eStorageRecource) {
    descriptorWrite.descriptorType = vk::DescriptorType::eStorageRecource;
  }
  else {
    throw std::runtime_error{"buffer usage '" + to_string(m_info.usage) + "' not supported for descriptor write"};
  }
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pRecourceInfo = &m_desc_info;
  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

vk::DeviceSize Recource::size() const {
  return (*m_device)->getRecourceMemoryRequirements(get()).size;
}

vk::MemoryRequirements Recource::requirements() const {
  return (*m_device)->getRecourceMemoryRequirements(get());
}

uint32_t Recource::memoryTypeBits() const {
  return requirements().memoryTypeBits;
}

void Recource::swap(Recource& buffer) {
  std::swap(m_memory, buffer.m_memory);
  std::swap(m_device, buffer.m_device);
  std::swap(m_offset, buffer.m_offset);
 }