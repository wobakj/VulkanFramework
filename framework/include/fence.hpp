#ifndef FENCE_HPP
#define FENCE_HPP

#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;

using WrapperFence = Wrapper<vk::Fence, vk::FenceCreateInfo>;
class Fence : public WrapperFence {
 public:
  Fence();
  Fence(Device const& dev, vk::FenceCreateFlags const& usage = vk::FenceCreateFlagBits::eSignaled);
  Fence(Fence && dev);
  Fence(Fence const&) = delete;
  ~Fence();
  
  Fence& operator=(Fence const&) = delete;
  Fence& operator=(Fence&& dev);

  void swap(Fence& dev);
  
  void wait();
  void reset();

 private:
  void destroy() override;

  Device const* m_device;
};

#endif