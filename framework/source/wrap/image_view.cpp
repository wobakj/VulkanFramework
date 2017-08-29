#include "wrap/image_view.hpp"

#include "wrap/image.hpp"
#include "wrap/image_res.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"

#include <iostream>

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


  view_info.subresourceRange = resource_range;
  return view_info;
}

// vk::AccessFlags layout_to_access(vk::ImageLayout const& layout) {
//   if (layout == vk::ImageLayout::ePreinitialized) {
//     return vk::AccessFlagBits::eHostWrite;
//   }
//   else if (layout == vk::ImageLayout::eUndefined) {
//     return vk::AccessFlags{};
//   }
//   else if (layout == vk::ImageLayout::eTransferDstOptimal) {
//     return vk::AccessFlagBits::eTransferWrite;
//   }
//   else if (layout == vk::ImageLayout::eTransferSrcOptimal) {
//     return vk::AccessFlagBits::eTransferRead;
//   } 
//   else if (layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
//     return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
//   } 
//   else if (layout == vk::ImageLayout::eColorAttachmentOptimal) {
//     return vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
//   } 
//   else if (layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
//     // read but not from input attachment
//     return vk::AccessFlagBits::eShaderRead;
//   } 
//   else if (layout == vk::ImageLayout::ePresentSrcKHR) {
//     return vk::AccessFlagBits::eMemoryRead;
//   } 
//   else if (layout == vk::ImageLayout::eGeneral) {
//     return vk::AccessFlagBits::eInputAttachmentRead
//          | vk::AccessFlagBits::eShaderRead
//          | vk::AccessFlagBits::eShaderWrite
//          | vk::AccessFlagBits::eColorAttachmentRead
//          | vk::AccessFlagBits::eColorAttachmentWrite
//          | vk::AccessFlagBits::eDepthStencilAttachmentRead
//          | vk::AccessFlagBits::eDepthStencilAttachmentWrite
//          | vk::AccessFlagBits::eTransferRead
//          | vk::AccessFlagBits::eTransferWrite
//          | vk::AccessFlagBits::eHostRead
//          | vk::AccessFlagBits::eHostWrite
//          | vk::AccessFlagBits::eMemoryRead
//          | vk::AccessFlagBits::eMemoryWrite;
//   } 
//   else {
//     throw std::invalid_argument("unsupported layout for access mask!");
//     return vk::AccessFlags{};
//   }
// }

///////////////////////////////////////////////////////////////////////////////

ImageRange::ImageRange()
 :m_image{}
 ,m_range{}
{}

ImageRange::ImageRange(vk::Image const& image, vk::ImageSubresourceRange const& range)
 :m_image{image}
 ,m_range{range}
{}

ImageRange::operator vk::ImageSubresourceRange const&() const {
  return range();
}

vk::Image const& ImageRange::image() const {
  return m_image;
}

vk::ImageSubresourceRange const& ImageRange::range() const {
  return m_range;
}

void ImageRange::swap(ImageRange& rhs) {
  std::swap(m_image, rhs.m_image);
  std::swap(m_range, rhs.m_range);
}

ImageLayers ImageRange::layers(unsigned layer_base, unsigned count_layers, unsigned mip_level) const {
  vk::ImageSubresourceLayers layers{};
  layers.aspectMask = m_range.aspectMask;
  layers.layerCount = count_layers;
  // layer and level relative to base values covered by view
  layers.baseArrayLayer = m_range.baseArrayLayer + layer_base;
  layers.mipLevel = m_range.baseMipLevel + mip_level;
  return ImageLayers{m_image, layers};
}

ImageLayers ImageRange::layers(unsigned mip_level) const {
  return layers(0, m_range.layerCount, mip_level);
}

ImageSubresource ImageRange::layer(unsigned layer, unsigned mip_level) const {
  vk::ImageSubresource subres{m_range.aspectMask, m_range.baseArrayLayer + layer, m_range.baseMipLevel + mip_level};  
  return ImageSubresource{m_image, subres};
}

///////////////////////////////////////////////////////////////////////////////

ImageLayers::ImageLayers()
 :m_image{}
 ,m_layers{}
{}

ImageLayers::ImageLayers(vk::Image const& image, vk::ImageSubresourceLayers const& range)
 :m_image{image}
 ,m_layers{range}
{}

ImageLayers::operator vk::ImageSubresourceLayers const&() const {
  return get();
}

ImageLayers::operator ImageRange() const {
  return range();
}

vk::Image const& ImageLayers::image() const {
  return m_image;
}

vk::ImageSubresourceLayers const& ImageLayers::get() const {
  return m_layers;
}

ImageRange ImageLayers::range() const {
  vk::ImageSubresourceRange range{m_layers.aspectMask, m_layers.mipLevel, 1, m_layers.baseArrayLayer, m_layers.layerCount};
  return ImageRange{m_image, range};
}

vk::ImageSubresource ImageLayers::layer(unsigned layer) const {
  return vk::ImageSubresource{m_layers.aspectMask, m_layers.baseArrayLayer + layer, m_layers.mipLevel};
}

///////////////////////////////////////////////////////////////////////////////

ImageSubresource::ImageSubresource()
 :m_image{}
 ,m_subres{}
{}

ImageSubresource::ImageSubresource(vk::Image const& image, vk::ImageSubresource const& subres)
 :m_image{image}
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

vk::Image const& ImageSubresource::image() const {
  return m_image;
}

vk::ImageSubresource const& ImageSubresource::get() const {
  return m_subres;
}

ImageRange ImageSubresource::range() const {
  vk::ImageSubresourceRange range{m_subres.aspectMask, m_subres.mipLevel, 1, m_subres.arrayLayer, 1};
  return ImageRange{m_image, range};
}

ImageLayers ImageSubresource::layers() const {
  vk::ImageSubresourceLayers layers{m_subres.aspectMask, m_subres.mipLevel, m_subres.arrayLayer, 1};
  return ImageLayers{m_image, layers};
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

ImageView::ImageView(BackedImage const& rhs)
 :WrapperImageView{}
 ,ImageRange{rhs.get(), img_to_view(rhs.get(), rhs.info()).subresourceRange}
{
  m_device = rhs.device();
  // m_image = rhs.get();
  m_image_info = rhs.info();
  m_info = img_to_view(image(), m_image_info); 
  // m_range = info().subresourceRange;
  m_object = m_device.createImageView(m_info);  
}

ImageView::ImageView(Device const& dev, vk::Image const& rhs, vk::ImageCreateInfo const& img_info)
 :WrapperImageView{}
 ,ImageRange{rhs, img_to_view(rhs, img_info).subresourceRange}
 // :ImageView{}
{
  m_device = dev.get();
  // m_image = rhs;
  m_image_info = img_info;
  m_info = img_to_view(image(), m_image_info); 
  // m_range = info().subresourceRange;
  m_object = m_device.createImageView(m_info);  
}

ImageView::ImageView(Image const& rhs)
 :WrapperImageView{}
 ,ImageRange{rhs.obj(), img_to_view(rhs.obj(), rhs.info()).subresourceRange}
{
  m_device = rhs.device();
  // m_image = rhs.obj();
  m_image_info = rhs.info();
  m_info = img_to_view(image(), m_image_info); 
  // m_range = info().subresourceRange;
  m_object = m_device.createImageView(m_info);  
}

// ImageView::ImageView(Device const& dev, vk::Image )
//  :ImageView{}
// {
//   m_device = &dev;
//   m_image = rhs.get();
//   m_image_info = rhs.info();
//   m_info = img_to_view(m_image, m_image_info); 
//   m_object = m_device.createImageView(m_info);  
// }

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

vk::Extent3D const& ImageView::extent() const {
  return m_image_info.extent;
}

vk::AttachmentDescription ImageView::toAttachment(bool clear) const {
  return img_to_attachment(m_image_info, clear);
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
  std::swap(m_image_info, rhs.m_image_info);
}

void ImageView::layoutTransitionCommand(vk::CommandBuffer const& command_buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const {
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = layout_old;
  barrier.newLayout = layout_new;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image();
  barrier.subresourceRange = range();

  barrier.srcAccessMask = layout_to_access(layout_old);
  barrier.dstAccessMask = layout_to_access(layout_new);

  command_buffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
}