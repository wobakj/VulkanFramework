#ifndef COMMAND_BUFFER_HPP
#define COMMAND_BUFFER_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;
class CommandPool;

using WrapperCommandBuffer = Wrapper<vk::CommandBuffer, vk::CommandBufferAllocateInfo>;
class CommandBuffer : public WrapperCommandBuffer {
 public:
  CommandBuffer();
  CommandBuffer(Device const& device, vk::CommandBuffer&& buffer, vk::CommandBufferAllocateInfo const& info);
  CommandBuffer(CommandBuffer && rhs);
  CommandBuffer(CommandBuffer const&) = delete;
  ~CommandBuffer();
  
  CommandBuffer& operator=(CommandBuffer const&) = delete;
  CommandBuffer& operator=(CommandBuffer&& rhs);

  void swap(CommandBuffer& rhs);
  
 private:
  CommandBuffer(CommandPool const& pool, uint32_t idx_queue, vk::CommandBufferLevel const& level);
  void destroy() override;

  Device const* m_device;
};

#endif