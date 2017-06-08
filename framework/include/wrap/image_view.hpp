#ifndef IMAGE_VIEW_HPP
#define IMAGE_VIEW_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory_resource.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;
class Image;
class ImageRes;

vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);

using WrapperImageView = Wrapper<vk::ImageView, vk::ImageViewCreateInfo>;
class ImageView : public WrapperImageView {
 public:
  
  ImageView();
  ImageView(ImageRes const& rhs); 
  ImageView(Image const& rhs); 
  ImageView(Device const& dev, vk::Image const& rhs, vk::ImageCreateInfo const& img_info);

  virtual ~ImageView();

  ImageView(ImageView && dev);
  ImageView(ImageView const&) = delete;
  
  ImageView& operator=(ImageView const&) = delete;
  ImageView& operator=(ImageView&& dev);

  operator vk::ImageSubresourceRange const&() const;

  virtual void layoutTransitionCommand(vk::CommandBuffer const& buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const;

  void swap(ImageView& dev);

  vk::Image const& image() const;
  vk::AttachmentDescription toAttachment(bool clear = true) const;
  vk::Format const& format() const;
  vk::Extent3D const& extent() const;
  vk::ImageSubresourceLayers layers(unsigned layer, unsigned count, unsigned mip_level = 0) const;
  vk::ImageSubresourceLayers layer(unsigned layer = 0, unsigned mip_level = 0) const;

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