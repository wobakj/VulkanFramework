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

void MappableResource::bindTo(Memory& memory, vk::DeviceSize const& offset) {
  m_offset = offset;
  m_memory = memory.get();
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

///////////////////////////////////////////////////////////

MemoryResource::MemoryResource()
 :MappableResource{}
 ,m_alloc{nullptr}
{}

MemoryResource::~MemoryResource() {
  if (m_mapped) {
    unmap();
  }
};

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
