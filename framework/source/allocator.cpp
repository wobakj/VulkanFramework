#include "allocator.hpp"

#include "wrap/device.hpp"

#include <utility>

Allocator::Allocator()
 :m_device{nullptr}
 ,m_type_index{0}
{}

Allocator::Allocator(Device const& device, uint32_t type_index)
 :m_device{&device}
 ,m_type_index{type_index}
{}

void Allocator::swap(Allocator& rhs) {
  std::swap(m_device, rhs.m_device);
  std::swap(m_type_index, rhs.m_type_index);
}
