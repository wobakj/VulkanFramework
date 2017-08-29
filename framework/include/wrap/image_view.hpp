#ifndef IMAGE_VIEW_HPP
#define IMAGE_VIEW_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;
class Image;
class BackedImage;

// +extent & offset
// create info - format, levels, layers, extent
// view        - range + img + type -- descriptor
// subresrange - mult miplevels --barrier
// subreslayers - 1 miplevel --transfer
// subres - 1 layer - linear memory layout & sparse

// base class ob subresource classes
// representing a certain region of a certain image
class ImageRegion {
 public:
  ImageRegion();
  ImageRegion(vk::Image const& image, vk::Extent3D const& extent, vk::Offset3D const& offset = vk::Offset3D{});
  // make abstract
  virtual ~ImageRegion() = 0;

  vk::Image const& image() const;
  vk::Extent3D const& extent() const;
  vk::Offset3D const& offset() const;

 protected:
  void swap(ImageRegion& rhs);

 private:
  vk::Image m_image;
  vk::Extent3D m_extent;
  vk::Offset3D m_offset;
};

class ImageLayers;
class ImageSubresource;

// a range, multiple layers and mip levels, for barriers 
class ImageRange : public ImageRegion {
 public:
  ImageRange();
  ImageRange(vk::Image const& image, vk::ImageSubresourceRange const& range, vk::Extent3D const& extent, vk::Offset3D const& offset = vk::Offset3D{});

  operator vk::ImageSubresourceRange const&() const;
  // assume mip level 0 for convenience
  operator ImageLayers() const;

  // per default, return all layers of base level
  ImageLayers layers(uint32_t mip_level = 0) const; 
  ImageLayers layers(unsigned layer, unsigned count, unsigned mip_level = 0) const;
  ImageSubresource layer(unsigned layer = 0, unsigned mip_level = 0) const;

  void layoutTransitionCommand(vk::CommandBuffer const& buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const;

 protected:
  vk::ImageSubresourceRange const& range() const;
  void swap(ImageRange& rhs);

 private:
  vk::ImageSubresourceRange m_range;
};

// one mip level, multiple layers, for data transfer 
class ImageLayers : public ImageRegion {
 public:
  ImageLayers();
  ImageLayers(vk::Image const& image, vk::ImageSubresourceLayers const& layers, vk::Extent3D const& extent, vk::Offset3D const& offset = vk::Offset3D{});

  operator vk::ImageSubresourceLayers const&() const;
  operator ImageRange() const;

  vk::ImageSubresource layer(unsigned layer = 0) const;
  ImageRange range() const;

 protected:
  vk::ImageSubresourceLayers const& get() const;

 private:
  vk::Image m_image;
  vk::ImageSubresourceLayers m_layers;
};

// one mip level and layer, for sparse and linear memory 
class ImageSubresource : public ImageRegion {
 public:
  ImageSubresource();
  ImageSubresource(vk::Image const& image, vk::ImageSubresource const& layers, vk::Extent3D const& extent, vk::Offset3D const& offset = vk::Offset3D{});

  operator vk::ImageSubresource const&() const;
  operator ImageLayers() const;
  operator ImageRange() const;

  ImageLayers layers() const;
  ImageRange range() const;

 protected:
  vk::ImageSubresource const& get() const;

 private:
  vk::Image m_image;
  vk::ImageSubresource m_subres;
};

///////////////////////////////////////////////////////////////////////////////

vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);

// the view of an ImageRange to bind to a descriptor
using WrapperImageView = Wrapper<vk::ImageView, vk::ImageViewCreateInfo>;
class ImageView : public WrapperImageView, public ImageRange {
 public:
  
  ImageView();
  ImageView(BackedImage const& rhs); 
  ImageView(Image const& rhs); 
  ImageView(Device const& dev, vk::Image const& rhs, vk::ImageCreateInfo const& img_info);

  virtual ~ImageView();

  ImageView(ImageView && dev);
  ImageView(ImageView const&) = delete;
  
  ImageView& operator=(ImageView const&) = delete;
  ImageView& operator=(ImageView&& dev);

  void swap(ImageView& dev);

  vk::AttachmentDescription toAttachment(bool clear = true) const;
  vk::Format const& format() const;
  vk::Extent3D const& extent() const;

  // write as combined sampler
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::Sampler const& sampler, uint32_t index = 0) const;
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::ImageLayout const& layout, vk::Sampler const& sampler, uint32_t index = 0) const;
  // write as input attachment
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::ImageLayout const& layout, vk::DescriptorType const& type, uint32_t index = 0) const;
 
 protected:
  void destroy() override;

  vk::Device m_device;

  vk::ImageCreateInfo m_image_info;
  
  friend class Transferrer;
};

#endif