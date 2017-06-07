#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/image_view.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

bool is_depth(vk::Format const& format);
bool has_stencil(vk::Format const& format);

vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info);
vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level = 0);

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout);
// vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);
vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features);

class Image {
 public:
  
  Image();
  Image(Device const& device, vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage); 
  virtual ~Image() {};

  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  virtual void layoutTransitionCommand(vk::CommandBuffer const& buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new);

  void swap(Image& dev);

  virtual vk::ImageLayout const& layout() const;
  virtual vk::AttachmentDescription toAttachment(bool clear = true) const;
  virtual vk::Format const& format() const;
  virtual ImageView const& view() const;
  virtual vk::Extent3D const& extent() const;
  // write as combined sampler
  virtual void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::Sampler const& sampler, uint32_t index = 0) const;
  // write as input attachment
  virtual void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;

  virtual vk::ImageCreateInfo const& info() const = 0;
 
 protected:
  // virtual vk::ImageCreateInfo& info() = 0;
  virtual vk::Image const& obj() const = 0;
  virtual Device const& device() const = 0;
  virtual void createView();

  ImageView m_view;
  friend class Transferrer;
  friend class ImageView;
};

#endif