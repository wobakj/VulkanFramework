#ifndef STATIC_ALLOCATOR_HPP
#define STATIC_ALLOCATOR_HPP

#include "wrap/device.hpp"
#include "wrap/memory.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <list>
#include <cmath>
#include <iostream>


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
  } handle;
};

static inline bool operator==(res_handle_t const& a, res_handle_t const& b) {
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
static inline bool operator!=(res_handle_t const& a, res_handle_t const& b) {
  return !(a == b);
}

static inline bool operator<(res_handle_t const& a, res_handle_t const& b) {
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

class StaticAllocator {
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
	StaticAllocator(Device const& device, uint32_t type_index, uint32_t block_bytes);
  StaticAllocator(StaticAllocator && rhs);
  StaticAllocator(StaticAllocator const&) = delete;

  StaticAllocator& operator=(StaticAllocator&& rhs);
  StaticAllocator& operator=(StaticAllocator const&) = delete;

  void swap(StaticAllocator& rhs);

  template<typename T, typename U>
  void allocate(MemoryResource<T, U>& resource);

  template<typename T, typename U>
  void free(MemoryResource<T, U>& resource);
  Memory const& mem() const {
    return m_block;
  }
 private:

  iterator_t findMatchingRange(vk::MemoryRequirements const& requirements);

  template<typename T, typename U>
  void addResource(MemoryResource<T, U>& resource, range_t const& range);

  Device const* m_device;
  uint32_t m_type_index;
  uint32_t m_block_bytes;

  Memory m_block;
  // list of per-block free ranges with offset and size
  std::list<range_t> m_free_ranges;
  std::map<res_handle_t, range_t> m_used_ranges;
};
  

inline StaticAllocator::StaticAllocator()
 :m_device{nullptr}
 ,m_type_index{0}
 ,m_block_bytes{0}
 ,m_free_ranges{}
 ,m_used_ranges{}
{}

inline StaticAllocator::StaticAllocator(Device const& device, uint32_t type_index, uint32_t block_bytes)
 :m_device{&device}
 ,m_type_index{type_index}
 ,m_block_bytes{block_bytes}
 ,m_block{*m_device, m_type_index, m_block_bytes}
 ,m_free_ranges(1, range_t{0u, m_block_bytes})
 ,m_used_ranges{}
{}

inline StaticAllocator::StaticAllocator(StaticAllocator && rhs)
 :StaticAllocator{}
{
  swap(rhs);
}

inline StaticAllocator& StaticAllocator::operator=(StaticAllocator&& rhs) {
  swap(rhs);
  return *this;
}

void inline StaticAllocator::swap(StaticAllocator& rhs) {
  std::swap(m_device, rhs.m_device);
  std::swap(m_type_index, rhs.m_type_index);
  std::swap(m_block_bytes, rhs.m_block_bytes);
  std::swap(m_block, rhs.m_block);
  std::swap(m_free_ranges, rhs.m_free_ranges);
  std::swap(m_used_ranges, rhs.m_used_ranges);
}

template<typename T, typename U>
inline void StaticAllocator::addResource(MemoryResource<T, U>& resource, range_t const& range) {
  resource.bindTo(m_block, range.offset);
  resource.bindTo(*this);
  m_used_ranges.emplace(res_handle_t{resource.get()}, range);
}

inline StaticAllocator::iterator_t StaticAllocator::findMatchingRange(vk::MemoryRequirements const& requirements) {
  for(auto iter_range = m_free_ranges.begin(); iter_range != m_free_ranges.end(); ++iter_range) {
    // round offset to alignment
    auto offset_align = requirements.alignment * vk::DeviceSize(std::ceil(float(iter_range->offset) / float(requirements.alignment)));
    if (requirements.size <= iter_range->size - (offset_align - iter_range->offset)) {
      return iter_range;
    }
  }
  return m_free_ranges.end();
}

template<typename T, typename U>
void StaticAllocator::allocate(MemoryResource<T, U>& resource) {
  auto const& requirements = resource.requirements();
  // check if block supports requirements
  if (!index_matches_filter(m_type_index, requirements.memoryTypeBits)) {
    throw std::runtime_error{"allocator memory type not suitable for object"};
  }

  if (requirements.size > m_block_bytes) {
    throw std::runtime_error{"resource size of " + std::to_string(requirements.size) + " larger than block size of " + std::to_string(m_block_bytes)};
  }
  auto iter_range = findMatchingRange(resource.requirements());
  
  // found matching range
  if (iter_range != m_free_ranges.end()) {
    auto offset_align = requirements.alignment * vk::DeviceSize(std::ceil(float(iter_range->offset) / float(requirements.alignment)));
    addResource(resource, range_t{offset_align, requirements.size});
    // object ends at end of range
    if (requirements.size + offset_align == iter_range->size + iter_range->offset) {
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
        m_free_ranges.emplace_back(iter_range->block, iter_range->offset, offset_align - iter_range->offset);
      }
      // remaining size
      iter_range->size = (iter_range->size + iter_range->offset) - (offset_align + requirements.size);
      // new offset
      iter_range->offset = offset_align + requirements.size;
    }
    // std::cout << resource.get() << " allocating range " << iter_range->block << ": " << offset_align << " - " << offset_align + requirements.size << std::endl;
  }
  else {
    throw std::length_error{"out of memory"};
  }
}

template<typename T, typename U>
void StaticAllocator::free(MemoryResource<T, U>& resource) {
  auto handle = res_handle_t{resource.get()};
  auto iter_object = m_used_ranges.find(handle);
  if (iter_object == m_used_ranges.end()) {
    throw std::runtime_error{"resource not found"};
  }
  auto const& range_object = iter_object->second;
  // std::cout << resource.get() << " freeing range " << range_object.block << ": " << range_object.offset << " - " << range_object.offset + range_object.size << std::endl;
  // find free ranges left and right of object
  iterator_t iter_range_l = m_free_ranges.end();
  iterator_t iter_range_r = m_free_ranges.end();
  for (auto iter_range = m_free_ranges.begin(); iter_range != m_free_ranges.end(); ++iter_range) {
    if (iter_range->offset == range_object.offset + range_object.size) {
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
      m_free_ranges.erase(iter_range_l);
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
  // remove object from used list
  m_used_ranges.erase(iter_object);
}

#endif