#ifndef MEMORY_RESOURCE_HPP
#define MEMORY_RESOURCE_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;
class Allocator;

// to store buffer and image handles in a map
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

bool operator==(res_handle_t const& a, res_handle_t const& b);
bool operator!=(res_handle_t const& a, res_handle_t const& b);
bool operator<(res_handle_t const& a, res_handle_t const& b);

// for directly backed resources (buffer, imeage) and subresources (buffer view)
class MappableResource {
 public:
  MappableResource();
  virtual ~MappableResource();

  virtual void bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset);

  void swap(MappableResource& rhs);

  virtual vk::DeviceSize size() const = 0;

  virtual void* map();
  virtual void* map(vk::DeviceSize const& size, vk::DeviceSize const& offset);
  virtual void unmap();
  virtual void setData(void const* data);
  virtual void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset);

 protected:
  vk::DeviceSize const& offset() const;
  vk::DeviceMemory const& memory() const;

  Device const* m_device;
  vk::DeviceMemory m_memory;
 private:
  vk::DeviceSize m_offset;
  bool m_mapped;
};
// for allocator interface
class MemoryResource :public MappableResource {
 public:
  MemoryResource();
  virtual ~MemoryResource() {};

  void free();
  
  void setAllocator(Allocator& memory);

  void swap(MemoryResource& rhs);

  virtual vk::MemoryRequirements requirements() const = 0;

  vk::DeviceSize alignment() const;
  vk::DeviceSize size() const override;
  uint32_t memoryTypeBits() const;

  virtual res_handle_t handle() const = 0;

 protected:
  Allocator* m_alloc;
};
// for directly backed resources (buffer, image)
template<typename T, typename U>
class MemoryResourceT : public Wrapper<T, U>, public MemoryResource {
  using WrapperMemoryResource = Wrapper<T, U>;
 public:
  MemoryResourceT()
   :WrapperMemoryResource{}
   ,MemoryResource{}
  {}

  // freeing of resources, implemented in derived class
  virtual void destroy() = 0;

  // overwrite cleanup to perform additional destruction
  virtual void cleanup() override {
    if (WrapperMemoryResource::m_object) {
      MemoryResource::free();
      destroy();
    }
  }

  void swap(MemoryResourceT& rhs) {
    WrapperMemoryResource::swap(rhs);
    MemoryResource::swap(rhs);
  }

};

#endif