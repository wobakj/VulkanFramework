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

///////////////////////////////////////////////////////////

MemoryResource::MemoryResource()
 :m_device{nullptr}
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
  std::swap(m_device, rhs.m_device);
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
