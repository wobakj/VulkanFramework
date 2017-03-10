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
  MappableResource()
   :m_device{nullptr}
   ,m_memory{}
   ,m_offset{0}
   ,m_mapped{false}
  {}

  virtual ~MappableResource() {
    if (m_mapped) {
      unmap();
    }
  };

  virtual void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void swap(MappableResource& rhs) {
    std::swap(m_device, rhs.m_device);
    std::swap(m_memory, rhs.m_memory);
    std::swap(m_offset, rhs.m_offset);
    std::swap(m_mapped, rhs.m_mapped);
  }

  virtual vk::DeviceSize size() const = 0;

  virtual void* map();
  virtual void* map(vk::DeviceSize const& size, vk::DeviceSize const& offset);
  virtual void unmap();
  virtual void setData(void const* data);
  virtual void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset);

 protected:
  Device const* m_device;
  vk::DeviceMemory m_memory;
  vk::DeviceSize m_offset;
  bool m_mapped;
};
// for allocator interface
class MemoryResource :public MappableResource {
 public:
  MemoryResource()
   :MappableResource{}
   ,m_alloc{nullptr}
  {}

  virtual ~MemoryResource() {
    if (m_mapped) {
      unmap();
    }
  };

  void free();
  
  void setAllocator(Allocator& memory);

  // virtual void bindTo(Memory& memory, vk::DeviceSize const& offset);
  void setMemory(Memory& memory);

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

  virtual ~MemoryResourceT() {};

  void swap(MemoryResourceT& rhs) {
    WrapperMemoryResource::swap(rhs);
    MemoryResource::swap(rhs);
  }

};

#endif