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

void MemoryResource::free() {
  if (m_alloc) {
    m_alloc->free(*this);
  }
}

void MemoryResource::setAllocator(Allocator& alloc) {
  m_alloc = &alloc;
}

void MemoryResource::bindTo(Memory& memory) {
  m_offset = memory.bindOffset(requirements());
  m_memory = memory.get();
}

void MemoryResource::bindTo(Memory& memory, vk::DeviceSize const& offset) {
  m_offset = memory.bindOffset(requirements(), offset);
  m_memory = memory.get();
}

void MemoryResource::setMemory(Memory& memory) {
  m_memory = memory.get();
}

void* MemoryResource::map(vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  return (*m_device)->mapMemory(m_memory, m_offset + offset, size);
}

void* MemoryResource::map() {
  return (*m_device)->mapMemory(m_memory, m_offset, size());
}

void MemoryResource::unmap() {
  (*m_device)->unmapMemory(m_memory);
}

void MemoryResource::setData(void const* data) {
  void* ptr = map();
  std::memcpy(ptr, data, size_t(size()));
  unmap();
}

void MemoryResource::setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset) {
  void* ptr = map(size, offset);
  std::memcpy(ptr, data, size_t(size));
  unmap();
}