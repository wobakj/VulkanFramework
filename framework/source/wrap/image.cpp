#include "wrap/image.hpp"
#include "wrap/image_view.hpp"

#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"
#include "wrap/memory.hpp"

#include <iostream>

bool is_depth(vk::Format const& format) {
  return format == vk::Format::eD32Sfloat
      || format == vk::Format::eD32SfloatS8Uint
      || format == vk::Format::eD24UnormS8Uint
      || format == vk::Format::eD16Unorm;
}

bool has_stencil(vk::Format const& format) {
  return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info) {
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

vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level) {
  vk::ImageSubresourceLayers layer{};
  auto range = img_to_resource_range(img_info);
  layer.aspectMask = range.aspectMask;
  layer.layerCount = range.layerCount;
  layer.baseArrayLayer = range.baseArrayLayer;
  layer.mipLevel = mip_level;
  return layer;
}

// vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info) {
//   vk::ImageViewCreateInfo view_info{};
//   view_info.image = image;
//   if (img_info.imageType == vk::ImageType::e1D) {
//     view_info.viewType = vk::ImageViewType::e1D;
//   }
//   else if(img_info.imageType == vk::ImageType::e2D) {
//     view_info.viewType = vk::ImageViewType::e2D;
//   }
//   else if (img_info.imageType == vk::ImageType::e3D){
//     view_info.viewType = vk::ImageViewType::e3D;
//   }
//   else throw std::runtime_error("wrong image format");

//   view_info.format = img_info.format;

//   view_info.components.r = vk::ComponentSwizzle::eIdentity;
//   view_info.components.g = vk::ComponentSwizzle::eIdentity;
//   view_info.components.b = vk::ComponentSwizzle::eIdentity;
//   view_info.components.a = vk::ComponentSwizzle::eIdentity;

//   view_info.subresourceRange = img_to_resource_range(img_info);
//   return view_info;
// }

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout) {
  if (layout == vk::ImageLayout::ePreinitialized) {
    return vk::AccessFlagBits::eHostWrite;
  }
  else if (layout == vk::ImageLayout::eUndefined) {
    return vk::AccessFlags{};
  }
  else if (layout == vk::ImageLayout::eTransferDstOptimal) {
    return vk::AccessFlagBits::eTransferWrite;
  }
  else if (layout == vk::ImageLayout::eTransferSrcOptimal) {
    return vk::AccessFlagBits::eTransferRead;
  } 
  else if (layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  } 
  else if (layout == vk::ImageLayout::eColorAttachmentOptimal) {
    return vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  } 
  else if (layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    // read but not from input attachment
    return vk::AccessFlagBits::eShaderRead;
  } 
  else if (layout == vk::ImageLayout::ePresentSrcKHR) {
    return vk::AccessFlagBits::eMemoryRead;
  } 
  else if (layout == vk::ImageLayout::eGeneral) {
    return vk::AccessFlagBits::eInputAttachmentRead
         | vk::AccessFlagBits::eShaderRead
         | vk::AccessFlagBits::eShaderWrite
         | vk::AccessFlagBits::eColorAttachmentRead
         | vk::AccessFlagBits::eColorAttachmentWrite
         | vk::AccessFlagBits::eDepthStencilAttachmentRead
         | vk::AccessFlagBits::eDepthStencilAttachmentWrite
         | vk::AccessFlagBits::eTransferRead
         | vk::AccessFlagBits::eTransferWrite
         | vk::AccessFlagBits::eHostRead
         | vk::AccessFlagBits::eHostWrite
         | vk::AccessFlagBits::eMemoryRead
         | vk::AccessFlagBits::eMemoryWrite;
  } 
  else {
    throw std::invalid_argument("unsupported layout for access mask!");
    return vk::AccessFlags{};
  }
}


vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features) {
  // prefer optimal tiling
  for (auto const& format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);
    if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  for (auto const& format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
      return format;
    } 
  }
  throw std::runtime_error("failed to find supported format!");
  return vk::Format::eUndefined;
}

Image::Image()
 :m_view{}
{}

Image::Image(Image && dev)
 :Image{}
{
  swap(dev);
}

Image::~Image() {
  // cleanup();
}

void Image::destroy() {
  if (m_view) {
    device()->destroyImageView(m_view);
  }
  // device()->destroyImage(obj());
}

Image& Image::operator=(Image&& dev) {
  swap(dev);
  return *this;
}

vk::ImageLayout const& Image::layout() const {
  return info().initialLayout;
}

vk::ImageView const& Image::view() const {
  return m_view;
}

vk::Format const& Image::format() const {
  return info().format;
}

vk::Extent3D const& Image::extent() const {
  return info().extent;
}

vk::AttachmentDescription Image::toAttachment(bool clear) const {
  return img_to_attachment(info(), clear);
}

void Image::createView() {
  auto view_info = img_to_view(obj(), info()); 
  m_view = device()->createImageView(view_info);  
}

void Image::writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::Sampler const& sampler, uint32_t index) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = info().initialLayout;
  // imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView = m_view;
  imageInfo.sampler = sampler;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  device()->updateDescriptorSets({descriptorWrite}, 0);
}

void Image::writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = info().initialLayout;
  // if (type == vk::DescriptorType::eSampledImage) {
    // imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  // }
  // else if (type == vk::DescriptorType::eStorageImage) {
  //   imageInfo.imageLayout = vk::ImageLayout::eGeneral;
  // }
  // else {
  //   throw std::runtime_error{"descriptor type not supported"};
  // }
  imageInfo.imageView = m_view;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  device()->updateDescriptorSets({descriptorWrite}, 0);
}

void Image::swap(Image& dev) {
  std::swap(m_view, dev.m_view);
 }

void Image::layoutTransitionCommand(vk::CommandBuffer const& command_buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) {
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = layout_old;
  barrier.newLayout = layout_new;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = obj();

  if (is_depth(format())) {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(format())) {
      barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  }

  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = info().mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = info().arrayLayers;

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