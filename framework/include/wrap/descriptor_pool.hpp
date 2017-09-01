#ifndef DESCRIPTOR_POOL_HPP
#define DESCRIPTOR_POOL_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "wrap/descriptor_set.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;
class DescriptorSetLayout;

using WrapperDescriptorPool = Wrapper<vk::DescriptorPool, DescriptorPoolInfo>;
class DescriptorPool : public WrapperDescriptorPool {
 public:
  DescriptorPool();
  DescriptorPool(Device const& dev, DescriptorPoolInfo const& info);
  DescriptorPool(DescriptorPool && dev);
  DescriptorPool(DescriptorPool const&) = delete;
  ~DescriptorPool();
 
  std::vector<DescriptorSet> allocate(std::vector<DescriptorSetLayout> const& layouts) const;
  DescriptorSet allocate(DescriptorSetLayout const& layout) const;

  DescriptorPool& operator=(DescriptorPool const&) = delete;
  DescriptorPool& operator=(DescriptorPool&& dev);

  void swap(DescriptorPool& dev);
  
  void wait();
  void reset();

 private:
  void destroy() override;

  vk::Device m_device;
};

#endif