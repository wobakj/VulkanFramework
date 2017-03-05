#ifndef DESCRIPTOR_POOL_HPP
#define DESCRIPTOR_POOL_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_pool_info.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;

using WrapperDescriptorPool = Wrapper<vk::DescriptorPool, DescriptorPoolInfo>;
class DescriptorPool : public WrapperDescriptorPool {
 public:
  DescriptorPool();
  DescriptorPool(Device const& dev, DescriptorPoolInfo const& info);
  DescriptorPool(DescriptorPool && dev);
  DescriptorPool(DescriptorPool const&) = delete;
  ~DescriptorPool();
  
  std::vector<vk::DescriptorSet> allocate(Shader const& shader) const;
  vk::DescriptorSet allocate(Shader const& shader, uint32_t set) const;

  DescriptorPool& operator=(DescriptorPool const&) = delete;
  DescriptorPool& operator=(DescriptorPool&& dev);

  void swap(DescriptorPool& dev);
  
  void wait();
  void reset();

 private:
  void destroy() override;

  Device const* m_device;
};

#endif