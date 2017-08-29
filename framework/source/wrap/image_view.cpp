#include "wrap/image_view.hpp"

#include "wrap/image.hpp"
#include "wrap/image_res.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"

#include <iostream>

vk::ImageSubresourceRange img_to_range(vk::ImageCreateInfo const& img_info) {
  vk::ImageSubresourceRange resource_range{};

  if(is_depth(img_info.format)) {
    resource_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
    if(has_stencil(img_info.format)) {
      resource_range.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  }
  else {
    resource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
  }
  resource_range.baseMipLevel = 0;
  resource_range.levelCount = img_info.mipLevels;
  resource_range.baseArrayLayer = 0;
  resource_range.layerCount = img_info.arrayLayers;

  return resource_range;
}

vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info) {
  vk::ImageViewCreateInfo view_info{};
  view_info.image = image;
  if (img_info.imageType == vk::ImageType::e1D) {
    view_info.viewType = vk::ImageViewType::e1D;
  }
  else if(img_info.imageType == vk::ImageType::e2D) {
    view_info.viewType = vk::ImageViewType::e2D;
  }
  else if (img_info.imageType == vk::ImageType::e3D){
    view_info.viewType = vk::ImageViewType::e3D;
  }
  else throw std::runtime_error("wrong image format");

  view_info.format = img_info.format;

  view_info.components.r = vk::ComponentSwizzle::eIdentity;
  view_info.components.g = vk::ComponentSwizzle::eIdentity;
  view_info.components.b = vk::ComponentSwizzle::eIdentity;
  view_info.components.a = vk::ComponentSwizzle::eIdentity;

  view_info.subresourceRange = img_to_range(img_info);
  return view_info;
}

///////////////////////////////////////////////////////////////////////////////
ImageRegion::~ImageRegion() {}

ImageRegion::ImageRegion()
 :m_image{}
 ,m_extent{}
 ,m_offset{}
{}

ImageRegion::ImageRegion(vk::Image const& image, vk::Extent3D const& extent, vk::Offset3D const& offset)
 :m_image{image}
 ,m_extent{extent}
 ,m_offset{offset}
{}

void ImageRegion::swap(ImageRegion& rhs) {
  std::swap(m_image, rhs.m_image);
  std::swap(m_extent, rhs.m_extent);
  std::swap(m_offset, rhs.m_offset);
}

vk::Image const&  ImageRegion::image() const {
  return m_image;
}

vk::Extent3D const&  ImageRegion::extent() const {
  return m_extent;
}

vk::Offset3D const&  ImageRegion::offset() const {
  return m_offset;
}


ImageRange::ImageRange()
 :ImageRegion{}
 ,m_range{}
{}

ImageRange::ImageRange(vk::Image const& image, vk::ImageSubresourceRange const& range, vk::Extent3D const& extent, vk::Offset3D const& offset)
 :ImageRegion{image, extent, offset}
 ,m_range{range}
{}

ImageRange::operator vk::ImageSubresourceRange const&() const {
  return range();
}

ImageRange::operator ImageLayers() const {
  return layers();
}

vk::ImageSubresourceRange const& ImageRange::range() const {
  return m_range;
}

void ImageRange::swap(ImageRange& rhs) {
  ImageRegion::swap(rhs);
  std::swap(m_range, rhs.m_range);
}

ImageLayers ImageRange::layers(unsigned layer_base, unsigned count_layers, unsigned mip_level) const {
  vk::ImageSubresourceLayers layers{};
  layers.aspectMask = m_range.aspectMask;
  layers.layerCount = count_layers;
  // layer and level relative to base values covered by view
  layers.baseArrayLayer = m_range.baseArrayLayer + layer_base;
  layers.mipLevel = m_range.baseMipLevel + mip_level;
  return ImageLayers{image(), layers, extent(), offset()};
}

ImageLayers ImageRange::layers(unsigned mip_level) const {
  return layers(0, m_range.layerCount, mip_level);
}

ImageSubresource ImageRange::layer(unsigned layer, unsigned mip_level) const {
  vk::ImageSubresource subres{m_range.aspectMask, m_range.baseArrayLayer + layer, m_range.baseMipLevel + mip_level};  
  return ImageSubresource{image(), subres, extent(), offset()};
}

///////////////////////////////////////////////////////////////////////////////

ImageLayers::ImageLayers()
 :ImageRegion{}
 ,m_layers{}
{}

ImageLayers::ImageLayers(vk::Image const& image, vk::ImageSubresourceLayers const& range, vk::Extent3D const& extent, vk::Offset3D const& offset)
 :ImageRegion{image, extent, offset}
 ,m_layers{range}
{}

ImageLayers::operator vk::ImageSubresourceLayers const&() const {
  return get();
}

ImageLayers::operator ImageRange() const {
  return range();
}

vk::ImageSubresourceLayers const& ImageLayers::get() const {
  return m_layers;
}

ImageRange ImageLayers::range() const {
  vk::ImageSubresourceRange range{m_layers.aspectMask, m_layers.mipLevel, 1, m_layers.baseArrayLayer, m_layers.layerCount};
  return ImageRange{image(), range, extent(), offset()};
}

vk::ImageSubresource ImageLayers::layer(unsigned layer) const {
  return vk::ImageSubresource{m_layers.aspectMask, m_layers.baseArrayLayer + layer, m_layers.mipLevel};
}

///////////////////////////////////////////////////////////////////////////////

ImageSubresource::ImageSubresource()
 :ImageRegion{}
 ,m_subres{}
{}

ImageSubresource::ImageSubresource(vk::Image const& image, vk::ImageSubresource const& subres, vk::Extent3D const& extent, vk::Offset3D const& offset)
 :ImageRegion{image, extent, offset}
 ,m_subres{subres}
{}

ImageSubresource::operator vk::ImageSubresource const&() const {
  return get();
}

ImageSubresource::operator ImageLayers() const {
  return layers();
}

ImageSubresource::operator ImageRange() const {
  return range();
}

vk::ImageSubresource const& ImageSubresource::get() const {
  return m_subres;
}

ImageRange ImageSubresource::range() const {
  vk::ImageSubresourceRange range{m_subres.aspectMask, m_subres.mipLevel, 1, m_subres.arrayLayer, 1};
  return ImageRange{image(), range, extent(), offset()};
}

ImageLayers ImageSubresource::layers() const {
  vk::ImageSubresourceLayers layers{m_subres.aspectMask, m_subres.mipLevel, m_subres.arrayLayer, 1};
  return ImageLayers{image(), layers, extent(), offset()};
}

///////////////////////////////////////////////////////////////////////////////

ImageView::ImageView()
 :WrapperImageView{}
 ,ImageRange{}
{}

ImageView::ImageView(ImageView && rhs)
 :ImageView{}
{
  swap(rhs);
}

ImageView::ImageView(Device const& dev, vk::Image const& rhs, vk::ImageCreateInfo const& img_info)
 :WrapperImageView{}
 ,ImageRange{rhs, img_to_range(img_info), img_info.extent}
 ,m_device{dev.get()}
{
  m_info = img_to_view(image(), img_info); 
  m_object = m_device.createImageView(m_info);  
}

ImageView::ImageView(Image const& rhs)
 :WrapperImageView{}
 ,ImageRange{rhs.obj(), img_to_range(rhs.info()), rhs.info().extent}
 ,m_device{rhs.device()}
{
  m_info = img_to_view(image(), rhs.info()); 
  m_object = m_device.createImageView(m_info);  
}

ImageView::~ImageView() {
  cleanup();
}

void ImageView::destroy() {
  m_device.destroyImageView(get());
}

ImageView& ImageView::operator=(ImageView&& rhs) {
  swap(rhs);
  return *this;
}

vk::Format const& ImageView::format() const {
  return info().format;
}

void ImageView::writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::Sampler const& sampler, uint32_t index) const {
  writeToSet(set, binding, is_depth(format()) ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal, sampler, index);
}

void ImageView::writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::DescriptorType const& type, uint32_t index) const {
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;
  // general layout is required for storage images
  if (type == vk::DescriptorType::eStorageImage) {
    layout = vk::ImageLayout::eGeneral;
  }
  else if (type == vk::DescriptorType::eInputAttachment) {
    if (is_depth(format())) {
      layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal; 
    }
    else {
      layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
  }
  else {
    throw std::runtime_error{"type not supported"};
  }
  writeToSet(set, binding, layout, type, index);
}

void ImageView::writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::ImageLayout const& layout, vk::Sampler const& sampler, uint32_t index) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = layout;
  imageInfo.imageView = get();
  imageInfo.sampler = sampler;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  m_device.updateDescriptorSets({descriptorWrite}, 0);
}

void ImageView::writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::ImageLayout const& layout, vk::DescriptorType const& type, uint32_t index) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = layout;
  imageInfo.imageView = get();

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  m_device.updateDescriptorSets({descriptorWrite}, 0);
}

void ImageView::swap(ImageView& rhs) {
  WrapperImageView::swap(rhs);
  ImageRange::swap(rhs);
  std::swap(m_device, rhs.m_device);
}
