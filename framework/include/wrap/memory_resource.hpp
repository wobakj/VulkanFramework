#ifndef MEMORY_RESOURCE_HPP
#define MEMORY_RESOURCE_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;
class Allocator;

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

bool operator==(res_handle_t const& a, res_handle_t const& b);
bool operator!=(res_handle_t const& a, res_handle_t const& b);
bool operator<(res_handle_t const& a, res_handle_t const& b);

class MemoryResource {
 public:
  MemoryResource()
   :m_device{nullptr}
   ,m_memory{nullptr}
   ,m_alloc{nullptr}
  {}

  virtual ~MemoryResource() {};

  void free();
  
  virtual void bindTo(Allocator& memory);
  virtual void bindTo(Memory& memory);
  virtual void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void setData(void const* data, vk::DeviceSize const& size, vk::DeviceSize const& offset = 0);

  void swap(MemoryResource& rhs) {
    std::swap(m_memory, rhs.m_memory);
    std::swap(m_device, rhs.m_device);
    std::swap(m_offset, rhs.m_offset);
    std::swap(m_alloc, rhs.m_alloc);
  }

  vk::DeviceSize size() const {
    return requirements().size;
  }

  vk::DeviceSize alignment() const {
    return requirements().alignment;
  }

  virtual vk::MemoryRequirements requirements() const = 0;

  uint32_t memoryTypeBits() const {
    return requirements().memoryTypeBits;
  }

  virtual res_handle_t handle() const = 0;

 protected:

  Device const* m_device;
  Memory* m_memory;
  Allocator* m_alloc;
  vk::DeviceSize m_offset;
};

template<typename T, typename U>
class MemoryResourceT : public Wrapper<T, U>, public MemoryResource {
  using WrapperMemoryResource = Wrapper<T, U>;
 public:
  MemoryResourceT()
   :WrapperMemoryResource{}
   ,MemoryResource{}
  {}

  void cleanup() override {
    MemoryResource::free();
    WrapperMemoryResource::cleanup();
  }

  virtual ~MemoryResourceT() {};

  void swap(MemoryResourceT& rhs) {
    WrapperMemoryResource::swap(rhs);
    MemoryResource::swap(rhs);
  }

};

#endif