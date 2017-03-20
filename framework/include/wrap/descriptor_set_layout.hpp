#ifndef DESCRIPTOR_SET_LAYOUT_HPP
#define DESCRIPTOR_SET_LAYOUT_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_set_layout_info.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

// class Device;

using WrapperDescriptorSetLayout = Wrapper<vk::DescriptorSetLayout, DescriptorSetLayoutInfo>;
class DescriptorSetLayout : public WrapperDescriptorSetLayout {
 public:
  DescriptorSetLayout();
  DescriptorSetLayout(vk::Device const& dev, DescriptorSetLayoutInfo const& info);
  DescriptorSetLayout(DescriptorSetLayout && dev);
  DescriptorSetLayout(DescriptorSetLayout const&) = delete;
  ~DescriptorSetLayout();

  DescriptorSetLayout& operator=(DescriptorSetLayout const&) = delete;
  DescriptorSetLayout& operator=(DescriptorSetLayout&& dev);

  void swap(DescriptorSetLayout& dev);
  
  void wait();
  void reset();

 private:
  void destroy() override;

  vk::Device m_device;
};

#endif