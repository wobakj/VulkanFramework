#include "allocator_block.hpp"

#include "wrap/device.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <cmath>
#include <iostream>

BlockAllocator::BlockAllocator()
 :Allocator{}
 ,m_block_bytes{0}
 ,m_free_ranges{}
 ,m_used_ranges{}
{}

BlockAllocator::BlockAllocator(Device const& device, uint32_t type_index, uint32_t block_bytes)
 :Allocator{device, type_index}
 ,m_block_bytes{block_bytes}
 ,m_free_ranges{}
 ,m_used_ranges{}
{}

BlockAllocator::BlockAllocator(BlockAllocator && rhs)
 :BlockAllocator{}
{
  swap(rhs);
}


BlockAllocator& BlockAllocator::operator=(BlockAllocator&& rhs) {
  swap(rhs);
  return *this;
}

void BlockAllocator::swap(BlockAllocator& rhs) {
  Allocator::swap(rhs);
  std::swap(m_block_bytes, rhs.m_block_bytes);
  std::swap(m_blocks, rhs.m_blocks);
  std::swap(m_free_ranges, rhs.m_free_ranges);
  std::swap(m_used_ranges, rhs.m_used_ranges);
}

void BlockAllocator::addResource(MemoryResource& resource, range_t const& range) {
  auto iter_block = m_blocks.begin(); 
  std::advance(iter_block, range.block);
  resource.bindTo(*iter_block, range.offset);
  resource.setAllocator(*this);
  m_used_ranges.emplace(res_handle_t{resource.handle()}, range);
}

BlockAllocator::iterator_t BlockAllocator::findMatchingRange(vk::MemoryRequirements const& requirements) {
  for(auto iter_range = m_free_ranges.begin(); iter_range != m_free_ranges.end(); ++iter_range) {
    // round offset to alignment
    auto offset_align = requirements.alignment * vk::DeviceSize(std::ceil(float(iter_range->offset) / float(requirements.alignment)));
    if (requirements.size <= iter_range->size - (offset_align - iter_range->offset)) {
      return iter_range;
    }
  }
  return m_free_ranges.end();
}

void BlockAllocator::addBlock() {
  m_free_ranges.emplace_back(range_t{uint32_t(m_blocks.size()), 0u, m_block_bytes});
  m_blocks.emplace_back(Memory(*m_device, m_type_index, m_block_bytes));
}

void BlockAllocator::allocate(MemoryResource& resource) {
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
    addResource(resource, range_t{iter_range->block, offset_align, requirements.size});
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
    // std::cout << resource.handle() << " allocating range " << iter_range->block << ": " << offset_align << " - " << offset_align + requirements.size << std::endl;
  }
  else {
    addBlock();
    addResource(resource, range_t{uint32_t(m_blocks.size()) - 1, 0, requirements.size});
    // update newly added free range
    m_free_ranges.back().offset = requirements.size;
    m_free_ranges.back().size -= requirements.size;
    // std::cout << resource.handle() << " allocating new range " << m_blocks.size() - 1 << ": " << 0 << " - " << requirements.size << std::endl;
  }
}

void BlockAllocator::free(MemoryResource& resource) {
  auto handle = res_handle_t{resource.handle()};
  auto iter_object = m_used_ranges.find(handle);
  if (iter_object == m_used_ranges.end()) {
    throw std::runtime_error{"resource not found"};
  }
  auto const& range_object = iter_object->second;
  // std::cout << resource.handle() << " freeing range " << range_object.block << ": " << range_object.offset << " - " << range_object.offset + range_object.size << std::endl;
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
