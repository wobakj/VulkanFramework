#ifndef BLOCK_ALLOCATOR_HPP
#define BLOCK_ALLOCATOR_HPP

#include "wrap/device.hpp"
#include "wrap/memory.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <list>
#include <cmath>

struct range_t {
  range_t(uint32_t b, uint32_t o, uint32_t s)
   :block{b}
   ,offset{o}
   ,size{s}
  {}

  uint32_t block;
  uint32_t offset;
  uint32_t size;
}; 
// to store any vulkan handle in a map
struct res_handle_t {
  res_handle_t(VkBuffer const& buf) {
    handle.buf = buf;
    i = false;
  }
  res_handle_t(VkImage const& img) {
    handle.img = img;
    i = true;
  }

  bool i;
  union handle {
    VkBuffer buf;
    VkImage img;
    uint64_t i;
  } handle;
};

static inline bool operator<(res_handle_t const& a, res_handle_t const& b) {
  if (a.i) {
    if (b.i) {
      return a.handle.i < b.handle.i;
    }
    else {
      return true;
    }
  }
  else {
    if (b.i) {
      return false;
    }
    else {
      return a.handle.i < b.handle.i;
    }
  }
}

using iterator_t = std::list<range_t>::iterator;

class BlockAllocator {
 public: 
  BlockAllocator()
   :m_device{nullptr}
   ,m_type_index{0}
   ,m_block_bytes{0}
  {}

	BlockAllocator(Device const& device, uint32_t type_index, uint32_t block_bytes)
   :m_device{&device}
   ,m_type_index{type_index}
   ,m_block_bytes{block_bytes}
  {}

  BlockAllocator(BlockAllocator && rhs)
   :BlockAllocator{}
  {
    swap(rhs);
  }
  BlockAllocator(BlockAllocator const&) = delete;

  BlockAllocator& operator=(BlockAllocator&& rhs) {
    swap(rhs);
    return *this;
  }

  void swap(BlockAllocator& rhs) {
    std::swap(m_device, rhs.m_device);
    std::swap(m_type_index, rhs.m_type_index);
    std::swap(m_block_bytes, rhs.m_block_bytes);
    std::swap(m_blocks, rhs.m_blocks);
    std::swap(m_free_ranges, rhs.m_free_ranges);
    std::swap(m_used_ranges, rhs.m_used_ranges);
  }

  template<typename T, typename U>
  void back(MemoryResource<T, U>& resource) {
    auto const& requirements = resource.requirements();
    auto iter_range = findMatchingRange(resource.requirements());
    
    // found matching range
    if (iter_range != m_free_ranges.end()) {
      auto offset_align = requirements.alignment * vk::DeviceSize(std::ceil(float(iter_range->offset) / float(requirements.alignment)));
      m_blocks[iter_range.block].bindResourceMemory(resource.get(), offset_align);
      m_used_ranges.emplace(resource.get(), range_t{iter_range.block, offset_align, requirements.size});
      // object ends at end of range
      if (requirements.size + offset_align == iter_range->size + offset_align - iter_range->offset) {
        // offset matches exactly, no more free space
        if (offset_align == iter_range->offset) {
          m_free_ranges.erase(iter_range);
        }
        else {
          iter_range->size = offset_align - iter_range->offset;
        }
      }
      // free space at end of range
      else {
        // free space in front of object
        if (offset_align != iter_range->offset) {
          // store free front space
          m_free_ranges.emplace_back(iter_range->block, iter_range->offset, iter_range->size = offset_align - iter_range->offset);
        }
        // remaining size
        iter_range->size = (iter_range->size + iter_range->offset) - (offset_align + requirements.size);
        // new offset
        iter_range->offset = offset_align + requirements.size;
      }
    }
    else {
      addBlock();
      m_blocks.back().bindResourceMemory(resource.get(), 0);
      m_used_ranges.emplace(resource.get(), range_t{m_blocks.size() - 1, 0, requirements.size});
      // update newly added free range
      m_free_ranges.back().offset = requirements.size;
      m_free_ranges.back().size -= requirements.size;
    }
  }

  void addBlock() {
    m_free_ranges.emplace_back(range_t{uint32_t(m_blocks.size()), 0u, m_block_bytes});
    m_blocks.emplace_back(Memory(*m_device, m_type_index, m_block_bytes));
  }

  template<typename T, typename U>
  void free(MemoryResource<T, U>& resource) {
    auto iter_object = m_used_ranges.find(res_handle_t{resource.get()});
    if (iter_object == m_used_ranges.end()) {
      throw std::runtime_error{"resource not found"};
    }
    auto const& range_object = iter_object->second;
    auto object_end = range_object.offset + range_object.size;
    // find free ranges left and right of object
    iterator_t iter_range_l{};
    iterator_t iter_range_r{};
    for (auto iter_range = m_free_ranges.begin(); iter_range != m_free_ranges.end(); ++iter_range) {
      if (iter_range->offset == object_end) {
        iter_range_r = iter_range;
        if (iter_range_l != m_free_ranges.end()) break;
      } 
      else if (iter_range->offset + iter_range->size == range_object.offset){
        iter_range_l = iter_range;
        if (iter_range_r != m_free_ranges.end()) break;
      }
    }
    // right range is free
    if (iter_range_r != m_free_ranges.end()) {
      // both ranges are free
      if (iter_range_l != m_free_ranges.end()) {
        iter_range_r->size = iter_range_r->offset + iter_range_r->size - iter_range_l->offset;
        iter_range_r->offset = iter_range_l->offset;
      }
      else {
        iter_range_r->size = iter_range_r->offset + iter_range_r->size - range_object.offset;
        iter_range_r->offset = range_object.offset;        
      }
    }
    // only left range is free
    else if (iter_range_l != m_free_ranges.end()) {
      iter_range_l->size = range_object.offset + range_object.size - iter_range_l->offset;
    }
    else {
      m_free_ranges.emplace_back(range_object);
    }
    // remove object form used list
    m_used_ranges.erase(iter_object);
  }

  iterator_t findMatchingRange(vk::MemoryRequirements const& requirements) {
    for(auto iter_range = m_free_ranges.begin(); iter_range != m_free_ranges.end(); ++iter_range) {
      // round offset to alignment
      auto offset_align = requirements.alignment * vk::DeviceSize(std::ceil(float(iter_range->offset) / float(requirements.alignment)));
      if (requirements.size <= iter_range->size - (offset_align - iter_range->offset)) {
        return iter_range;
      }
    }
    return iterator_t{};
  }

 private:
  Device const* m_device;
  uint32_t m_type_index;
  uint32_t m_block_bytes;

  std::vector<Memory> m_blocks;
  // list of per-block free ranges with offset and size
  std::list<range_t> m_free_ranges;
  std::map<res_handle_t, range_t> m_used_ranges;
};
  
#endif