#ifndef IMAGE_VIEW_HPP
#define IMAGE_VIEW_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;
class Image;
class ImageRes;

// bool is_depth(vk::Format const& format);
// bool has_stencil(vk::Format const& format);

// vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info);
// vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level = 0);

// vk::AccessFlags layout_to_access(vk::ImageLayout const& layout);
vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);
// vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features);

using WrapperImageView = Wrapper<vk::ImageView, vk::ImageViewCreateInfo>;
class ImageView : WrapperImageView {
 public:
  
  ImageView();
  ImageView(ImageRes const& usage); 
  virtual ~ImageView();

  ImageView(ImageView && dev);
  ImageView(ImageView const&) = delete;
  
  ImageView& operator=(ImageView const&) = delete;
  ImageView& operator=(ImageView&& dev);

  virtual void layoutTransitionCommand(vk::CommandBuffer const& buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new);

  void swap(ImageView& dev);

  vk::ImageLayout const& layout() const;
  vk::AttachmentDescription toAttachment(bool clear = true) const;
  vk::Format const& format() const;
  vk::Extent3D const& extent() const;
  // write as combined sampler
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::Sampler const& sampler, uint32_t index = 0) const;
  // write as input attachment
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;
 
 protected:
  void destroy() override;

  Device const* m_device;

  vk::Image m_image;
  vk::ImageCreateInfo m_image_info;
  
  friend class Transferrer;
};

#endif