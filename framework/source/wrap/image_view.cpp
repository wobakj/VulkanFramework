#include "wrap/image_view.hpp"

#include "wrap/image.hpp"
#include "wrap/image_res.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"

#include <iostream>

// vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info) {
//   vk::ImageSubresourceRange resource_range{};

//   if(is_depth(img_info.format)) {
//     resource_range.aspectMask = vk::ImageAspectFlagBits::eDepth;
//     if(has_stencil(img_info.format)) {
//       resource_range.aspectMask |= vk::ImageAspectFlagBits::eStencil;
//     }
//   }
//   else {
//     resource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
//   }
//   resource_range.baseMipLevel = 0;
//   resource_range.levelCount = img_info.mipLevels;
//   resource_range.baseArrayLayer = 0;
//   resource_range.layerCount = img_info.arrayLayers;

//   return resource_range;
// }

// vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level) {
//   vk::ImageSubresourceLayers layer{};
//   auto range = img_to_resource_range(img_info);
//   layer.aspectMask = range.aspectMask;
//   layer.layerCount = range.layerCount;
//   layer.baseArrayLayer = range.baseArrayLayer;
//   layer.mipLevel = mip_level;
//   return layer;
// }

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

ImageView::ImageView()
 :WrapperImageView{}
{}

ImageView::ImageView(ImageView && rhs)
 :ImageView{}
{
  swap(rhs);
}

ImageView::ImageView(BackedImage const& rhs)
 :ImageView{}
{
  m_device = rhs.device();
  m_image = rhs.get();
  m_image_info = rhs.info();
  m_info = img_to_view(m_image, m_image_info); 
  m_object = m_device.createImageView(m_info);  
}

ImageView::ImageView(Device const& dev, vk::Image const& rhs, vk::ImageCreateInfo const& img_info)
 :ImageView{}
{
  m_device = dev.get();
  m_image = rhs;
  m_image_info = img_info;
  m_info = img_to_view(m_image, m_image_info); 
  m_object = m_device.createImageView(m_info);  
}

ImageView::ImageView(Image const& rhs)
 :ImageView{}
{
  m_device = rhs.device();
  m_image = rhs.obj();
  m_image_info = rhs.info();
  m_info = img_to_view(m_image, m_image_info); 
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

ImageView::operator vk::ImageSubresourceRange const&() const {
  return info().subresourceRange;
}

vk::Image const& ImageView::image() const {
  return m_image;
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

vk::ImageSubresourceLayers ImageView::layers(unsigned layer, unsigned count, unsigned mip_level) const {
  vk::ImageSubresourceLayers layers{};
  auto range = info().subresourceRange;
  layers.aspectMask = range.aspectMask;
  layers.layerCount = count;
  // layer and level relative to base values covered by view
  layers.baseArrayLayer = range.baseArrayLayer + layer;
  layers.mipLevel =range.baseMipLevel + mip_level;
  return layers;
}
vk::ImageSubresourceLayers ImageView::layer(unsigned layer, unsigned mip_level) const {
  return layers(layer, 1, mip_level);
}

vk::ImageSubresourceLayers ImageView::layers(unsigned mip_level) const {
  return layers(0, info().subresourceRange.layerCount, mip_level);
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
  std::swap(m_device, rhs.m_device);
  std::swap(m_image, rhs.m_image);
  std::swap(m_image_info, rhs.m_image_info);
}

void ImageView::layoutTransitionCommand(vk::CommandBuffer const& command_buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const {
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = layout_old;
  barrier.newLayout = layout_new;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = m_image;
  barrier.subresourceRange = info().subresourceRange;

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