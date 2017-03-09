#ifndef BLOCK_ALLOCATOR_HPP
#define BLOCK_ALLOCATOR_HPP

#include "allocator.hpp"

#include "wrap/memory.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <list>

class Device;
class MemoryResource;

class BlockAllocator : public Allocator {
  struct range_t {
    range_t(uint32_t b, vk::DeviceSize o, vk::DeviceSize s)
     :block{b}
     ,offset{o}
     ,size{s}
    {}

    uint32_t block;
    vk::DeviceSize offset;
    vk::DeviceSize size;
  }; 

  using iterator_t = std::list<range_t>::iterator;
 public: 
  BlockAllocator();
	BlockAllocator(Device const& device, uint32_t type_index, uint32_t block_bytes);
  BlockAllocator(BlockAllocator && rhs);
  BlockAllocator(BlockAllocator const&) = delete;

  BlockAllocator& operator=(BlockAllocator&& rhs);
  BlockAllocator& operator=(BlockAllocator const&) = delete;

  void swap(BlockAllocator& rhs);

  void allocate(MemoryResource& resource);

  void free(MemoryResource& resource);

 private:
  void addBlock();

  iterator_t findMatchingRange(vk::MemoryRequirements const& requirements);

  void addResource(MemoryResource& resource, range_t const& range);

  uint32_t m_block_bytes;

  std::list<Memory> m_blocks;
  // list of per-block free ranges with offset and size
  std::list<range_t> m_free_ranges;
  std::map<res_handle_t, range_t> m_used_ranges;
};

#endif