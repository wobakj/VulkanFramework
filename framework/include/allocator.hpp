#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <cstdint>

class Device;
class MemoryResource;

class Allocator {
 public: 
  Allocator();
  Allocator(Device const& device, uint32_t type_index);
  virtual ~Allocator() {};
  void swap(Allocator& rhs);

  virtual void allocate(MemoryResource& resource) = 0;

  virtual void free(MemoryResource& resource) = 0;
  // get ptr to virtual address
  virtual uint8_t* map(MemoryResource& resource) = 0;
 protected:
  Device const* m_device;
  uint32_t m_type_index;
};

#endif