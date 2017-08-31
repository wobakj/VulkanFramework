#ifndef DESCRIPTOR_SET_HPP
#define DESCRIPTOR_SET_HPP

#include "wrap/wrapper.hpp"
#include "wrap/descriptor_set_layout_info.hpp"

#include <vulkan/vulkan.hpp>

class ImageView;

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

  // write as combined sampler
  void bind(uint32_t binding, uint32_t index, ImageView const& view, vk::Sampler const& sampler) const;
  void bind(uint32_t binding, uint32_t index, ImageView const& view, vk::ImageLayout const& layout, vk::Sampler const& sampler) const;
  // write as input attachment
  void bind(uint32_t binding, uint32_t index, ImageView const& view, vk::DescriptorType const& type) const;
  void bind(uint32_t binding, uint32_t index, ImageView const& view, vk::ImageLayout const& layout, vk::DescriptorType const& type) const;
  
  // convenience methods
  void bind(uint32_t binding, ImageView const& view, vk::Sampler const& sampler) const {
    bind(binding, 0, view, sampler);
  }
  void bind(uint32_t binding, ImageView const& view, vk::ImageLayout const& layout, vk::Sampler const& sampler) const {
    bind(binding, 0, view, layout, sampler);
  }
  void bind(uint32_t binding, ImageView const& view, vk::DescriptorType const& type) const {
    bind(binding, 0, view, type);
  }
  void bind(uint32_t binding, ImageView const& view, vk::ImageLayout const& layout, vk::DescriptorType const& type) const {
    bind(binding, 0, view, layout, type);
  }

 private:
  void destroy() override;

  vk::Device m_device;
  vk::DescriptorPool m_pool;
};

#endif