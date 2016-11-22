#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "wrapper.hpp"
#include "resource.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;
class Device;
class Memory;

using ResourceBuffer = Resource<vk::Buffer, vk::BufferCreateInfo>;
class Buffer : public ResourceBuffer {
 public:
  
  Buffer();
  Buffer(Device const& dev, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage);
  Buffer(Buffer && dev);
  Buffer(Buffer const&) = delete;
  ~Buffer();
  
  Buffer& operator=(Buffer const&) = delete;
  Buffer& operator=(Buffer&& dev);

  void bindTo(Memory& memory);
  void bindTo(Memory& memory, vk::DeviceSize const& offset);

  void swap(Buffer& dev);

  void writeToSet(vk::DescriptorSet& set, uint32_t binding, uint32_t index = 0) const override;

  vk::MemoryRequirements requirements() const;
 private:
  void destroy() override;

  vk::DescriptorBufferInfo m_desc_info;
};

#endif