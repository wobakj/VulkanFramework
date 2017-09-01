#ifndef COMMAND_POOL_HPP
#define COMMAND_POOL_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;
class CommandBuffer;

using WrapperCommandPool = Wrapper<vk::CommandPool, vk::CommandPoolCreateInfo>;
class CommandPool : public WrapperCommandPool {
 public:
  CommandPool();
  CommandPool(Device const& dev, uint32_t idx_queue, vk::CommandPoolCreateFlags const& flags);
  CommandPool(CommandPool && dev);
  CommandPool(CommandPool const&) = delete;
  ~CommandPool();
  
  CommandPool& operator=(CommandPool const&) = delete;
  CommandPool& operator=(CommandPool&& dev);

  void swap(CommandPool& dev);
  
  std::vector<CommandBuffer> createBuffers(vk::CommandBufferLevel const& level, uint32_t num) const;
  CommandBuffer createBuffer(vk::CommandBufferLevel const& level) const;

 private:
  void destroy() override;

  vk::Device m_device;
};

#endif