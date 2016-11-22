#include "image.hpp"

#include "render_pass.hpp"
#include "device.hpp"
#include "memory.hpp"

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

  view_info.subresourceRange = img_to_resource_range(img_info);
  return view_info;
}

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
    return vk::AccessFlags{};
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
 :WrapperImage{}
 ,m_device{nullptr}
 ,m_memory{nullptr}
 ,m_view{VK_NULL_HANDLE}
{}

Image::Image(Image && dev)
 :Image{}
{
  swap(dev);
}

Image::Image(Device const& device, std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags) 
 :Image{}
{  
  m_device = &device;

  m_info.imageType = vk::ImageType::e2D;
  m_info.extent.width = width;
  m_info.extent.height = height;
  m_info.extent.depth = 1;
  m_info.mipLevels = 1;
  m_info.arrayLayers = 1;
  m_info.format = format;
  m_info.tiling = tiling;
  // if memory is host visible, image is likely target of upload
  if (mem_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
    m_info.initialLayout = vk::ImageLayout::ePreinitialized;
  }
  else {
    m_info.initialLayout = vk::ImageLayout::eUndefined;
  }
  m_info.usage = usage;

  auto queueFamilies = device.ownerIndices();
  if (queueFamilies.size() > 1) {
    m_info.sharingMode = vk::SharingMode::eConcurrent;
    m_info.queueFamilyIndexCount = std::uint32_t(queueFamilies.size());
    m_info.pQueueFamilyIndices = queueFamilies.data();
  }
  else {
    m_info.sharingMode = vk::SharingMode::eExclusive;
  }

  m_object = device->createImage(info());
  m_flags_mem = mem_flags;
}

Image::~Image() {
  cleanup();
}

void Image::setData(void const* data, vk::DeviceSize const& size) {
  m_memory->setData(data, size, m_offset);
}

void Image::bindTo(Memory& memory) {
  m_offset = memory.bindImage(*this);
  m_memory = &memory;

  if ((info().usage ^ vk::ImageUsageFlagBits::eTransferSrc) &&
      (info().usage ^ vk::ImageUsageFlagBits::eTransferDst) &&
      (info().usage ^ (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc))) {
    createView();
  }
}

void Image::bindTo(Memory& memory, vk::DeviceSize const& offset) {
  m_offset = memory.bindImage(*this, offset);
  m_memory = &memory;

  if ((info().usage ^ vk::ImageUsageFlagBits::eTransferSrc) &&
      (info().usage ^ vk::ImageUsageFlagBits::eTransferDst) &&
      (info().usage ^ (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc))) {
    createView();
  }
}

void Image::transitionToLayout(vk::ImageLayout const& newLayout) {
  m_device->transitionToLayout(get(), info(), newLayout);
  // store new layout
  m_info.initialLayout = newLayout;
}

void Image::destroy() {
  if (m_view) {
    (*m_device)->destroyImageView(m_view);
  }
  (*m_device)->destroyImage(get());
}

 Image& Image::operator=(Image&& dev) {
  swap(dev);
  return *this;
 }

vk::DeviceSize Image::size() const {
  return (*m_device)->getImageMemoryRequirements(get()).size;
}

vk::ImageLayout const& Image::layout() const {
  return m_info.initialLayout;
}

vk::ImageView const& Image::view() const {
  return m_view;
}

vk::Format const& Image::format() const {
  return m_info.format;
}

vk::AttachmentDescription Image::toAttachment(bool clear) const {
  return img_to_attachment(info(), clear);
}

uint32_t Image::memoryType() const {
  return findMemoryType(m_device->physical(), requirements().memoryTypeBits, m_flags_mem);
}

uint32_t Image::memoryTypeBits() const {
  return requirements().memoryTypeBits;
}

void Image::createView() {
  auto view_info = img_to_view(get(), info()); 
  m_view = (*m_device)->createImageView(view_info);  
}

void Image::writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::Sampler const& sampler) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView = m_view;
  imageInfo.sampler = sampler;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

void Image::writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView = m_view;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = vk::DescriptorType::eInputAttachment;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

vk::MemoryRequirements Image::requirements() const {
  return (*m_device)->getImageMemoryRequirements(get());
}

vk::MemoryPropertyFlags const& Image::memFlags() const {
  return m_flags_mem;
}

void Image::swap(Image& dev) {
  WrapperImage::swap(dev);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_device, dev.m_device);
  std::swap(m_view, dev.m_view);
  std::swap(m_offset, dev.m_offset);
  std::swap(m_flags_mem, dev.m_flags_mem);
 }