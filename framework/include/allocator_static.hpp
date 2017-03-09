#ifndef STATIC_ALLOCATOR_HPP
#define STATIC_ALLOCATOR_HPP

#include "allocator.hpp"

#include "wrap/memory.hpp"

#include <vulkan/vulkan.hpp>

#include <map>
#include <list>

class Device;
class MemoryResource;

class StaticAllocator : public Allocator {
  struct range_t {
    range_t(vk::DeviceSize o, vk::DeviceSize s)
     :offset{o}
     ,size{s}
    {}

    vk::DeviceSize offset;
    vk::DeviceSize size;
  }; 

  using iterator_t = std::list<range_t>::iterator;
 public: 
  StaticAllocator();
	StaticAllocator(Device const& device, uint32_t type_index, size_t block_bytes);
  StaticAllocator(StaticAllocator && rhs);
  StaticAllocator(StaticAllocator const&) = delete;

  StaticAllocator& operator=(StaticAllocator&& rhs);
  StaticAllocator& operator=(StaticAllocator const&) = delete;

  void swap(StaticAllocator& rhs);

  void allocate(MemoryResource& resource);

  void free(MemoryResource& resource);
  Memory const& mem() const {
    return m_block;
  }
 private:

  iterator_t findMatchingRange(vk::MemoryRequirements const& requirements);

  void addResource(MemoryResource& resource, range_t const& range);

  Device const* m_device;
  uint32_t m_type_index;
  size_t m_block_bytes;

  Memory m_block;
  // list of per-block free ranges with offset and size
  std::list<range_t> m_free_ranges;
  std::map<res_handle_t, range_t> m_used_ranges;
};

#endif