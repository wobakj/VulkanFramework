#include "wrap/memory_resource.hpp"

#include "wrap/memory.hpp"
#include "wrap/device.hpp"
#include "allocator.hpp"

bool operator==(res_handle_t const& a, res_handle_t const& b) {
  if (a.i) {
    if (b.i) {
      return a.handle.img == b.handle.img;
    }
  }
  else {
    if (!b.i) {
      return a.handle.buf == b.handle.buf;
    }
  }
  return false;
}

bool operator!=(res_handle_t const& a, res_handle_t const& b) {
  return !(a == b);
}

bool operator<(res_handle_t const& a, res_handle_t const& b) {
  if (a.i) {
    if (b.i) {
      return a.handle.img < b.handle.img;
    }
    else {
      return false;
    }
  }
  else {
    if (b.i) {
      return true;
    }
    else {
      return a.handle.buf < b.handle.buf;
    }
  }
}

MappableResource::MappableResource()
 :m_device{nullptr}
 ,m_memory{}
 ,m_offset{0}
 ,m_mapped{false}
{}

MappableResource::~MappableResource() {
  if (m_mapped) {
    unmap();
  }
}

void MappableResource::swap(MappableResource& rhs) {
  std::swap(m_device, rhs.m_device);
  std::swap(m_memory, rhs.m_memory);
  std::swap(m_offset, rhs.m_offset);
  std::swap(m_mapped, rhs.m_mapped);
}

void MappableResource::bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset) {
  m_offset = offset;
  m_memory = memory;
}

void* MappableResource::map(vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  assert(!m_mapped);
  m_mapped = true;
  return (*m_device)->mapMemory(m_memory, m_offset + offset, size);
}

void* MappableResource::map() {
  return map(size(), 0);
}

void MappableResource::unmap() {
  assert(m_mapped);
  m_mapped = false;
  (*m_device)->unmapMemory(m_memory);
}

void MappableResource::setData(void const* data) {
  void* ptr = map();
  std::memcpy(ptr, data, size_t(size()));
  unmap();
}

void MappableResource::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  void* ptr = map(size, offset);
  std::memcpy(ptr, data, size_t(size));
  unmap();
}

vk::DeviceSize const& MappableResource::offset() const {
  return m_offset;
}

vk::DeviceMemory const& MappableResource::memory() const {
  return m_memory;
}

///////////////////////////////////////////////////////////

MemoryResource::MemoryResource()
 :MappableResource{}
 ,m_alloc{nullptr}
{}

void MemoryResource::free() {
  if (m_alloc) {
    m_alloc->free(*this);
  }
}

void MemoryResource::setAllocator(Allocator& alloc) {
  m_alloc = &alloc;
}

void MemoryResource::swap(MemoryResource& rhs) {
  MappableResource::swap(rhs);
  std::swap(m_alloc, rhs.m_alloc);
}

vk::DeviceSize MemoryResource::size() const {
  return requirements().size;
}

vk::DeviceSize MemoryResource::alignment() const {
  return requirements().alignment;
}

uint32_t MemoryResource::memoryTypeBits() const {
  return requirements().memoryTypeBits;
}
