#ifndef DESCRIPTOR_SET_HPP
#define DESCRIPTOR_SET_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_set_layout_info.hpp"

#include <vulkan/vulkan.hpp>

using WrapperDescriptorSet = Wrapper<vk::DescriptorSet, DescriptorSetLayoutInfo>;
class DescriptorSet : public WrapperDescriptorSet {
 public:
  DescriptorSet();
  // take ownership
  DescriptorSet(vk::Device const& dev, vk::DescriptorSet&& set, DescriptorSetLayoutInfo const& info, vk::DescriptorPool const& pool);
  DescriptorSet(DescriptorSet && dev);
  DescriptorSet(DescriptorSet const&) = delete;
  ~DescriptorSet();

  DescriptorSet& operator=(DescriptorSet const&) = delete;
  DescriptorSet& operator=(DescriptorSet&& dev);

  void swap(DescriptorSet& dev);
  
  void wait();
  void reset();

 private:
  void destroy() override;

  vk::Device m_device;
  vk::DescriptorPool m_pool;
};

#endif