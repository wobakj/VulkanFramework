#include "image.hpp"

#include "device.hpp"

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
  else if (layout == vk::ImageLayout::eTransferDstOptimal) {
    return vk::AccessFlagBits::eTransferWrite;
  } 
  else if (layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
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
// todo: support dependencies for attachments being reused as color/depth in target pass
vk::SubpassDependency img_to_dependency(vk::ImageCreateInfo const& img_info, int32_t src, int32_t dst) {
  vk::SubpassDependency dependency{};
  if(src < 0) {
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  }
  else {
    dependency.srcSubpass = src;
  }
  if(dst < 0) {
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
  }
  else {
    dependency.dstSubpass = dst;
  }

  if (img_info.usage & vk::ImageUsageFlagBits::eColorAttachment) {
    // present image, presentation needs to be done before writing new info
    if(src < 0) {
      dependency.dstStageMask |= vk::PipelineStageFlagBits::eBottomOfPipe;
      dependency.dstAccessMask |= vk::AccessFlagBits::eMemoryRead;
      dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
      return dependency;
    }
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  }
  else if (img_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  }
  else {
    throw std::runtime_error{"usage flags" + to_string(img_info.usage) + " not supported"};
  }

  // input attachment can ony be read in frag shader
  if (img_info.usage & vk::ImageUsageFlagBits::eInputAttachment) {
    dependency.dstStageMask |= vk::PipelineStageFlagBits::eFragmentShader;
    dependency.dstAccessMask |= vk::AccessFlagBits::eInputAttachmentRead;
  }
  // sampler can be read in next vertex stage already
  if (img_info.usage & vk::ImageUsageFlagBits::eSampled) {
    dependency.dstStageMask |= vk::PipelineStageFlagBits::eVertexShader;
    dependency.dstAccessMask |= vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  }
  // transfer image should be done until next transfer operation
  if (img_info.usage & vk::ImageUsageFlagBits::eTransferSrc) {
    dependency.dstStageMask |= vk::PipelineStageFlagBits::eTransfer;
    dependency.dstAccessMask |= vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite;
  }
  // storage is immutable sampler
  if (img_info.usage & vk::ImageUsageFlagBits::eStorage) {
    dependency.dstStageMask |= vk::PipelineStageFlagBits::eVertexShader;
    dependency.dstAccessMask |= vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  }
  // special case, presentation after renderpass, assumption may be too strong
  if (dst < 0 && dependency.dstAccessMask == vk::AccessFlags{}) {
    dependency.dstStageMask |= vk::PipelineStageFlagBits::eBottomOfPipe;
    dependency.dstAccessMask |= vk::AccessFlagBits::eMemoryRead;
  }

  if(dependency.dstAccessMask == vk::AccessFlags{}) {
    throw std::runtime_error{"usage flags" + to_string(img_info.usage) + " not supported"};
  }
  return dependency;
}

vk::AttachmentDescription img_to_attachment(vk::ImageCreateInfo const& img_info, bool clear) {
  vk::AttachmentDescription attachment{};
  attachment.format = img_info.format;
  if(clear) {
    attachment.loadOp = vk::AttachmentLoadOp::eClear;    
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
  }
  else {
    attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
  }
  // store data only when it is used afterwards
  if ((img_info.usage & vk::ImageUsageFlagBits::eSampled) 
   || (img_info.usage & vk::ImageUsageFlagBits::eStorage)
   || (img_info.usage & vk::ImageUsageFlagBits::eInputAttachment)
   || (img_info.usage & vk::ImageUsageFlagBits::eTransferSrc)
   || img_info.initialLayout == vk::ImageLayout::ePresentSrcKHR) {
    attachment.storeOp = vk::AttachmentStoreOp::eStore;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
  }
  else {
    attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  }
  // if image is cleared, initial layout doesnt matter as data is deleted 
  if (clear) {
    attachment.initialLayout = vk::ImageLayout::eUndefined;
  }
  else {
    attachment.initialLayout = img_info.initialLayout;
  }

  attachment.finalLayout = img_info.initialLayout;

  return attachment;
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
 ,m_memory{}
 ,m_device{nullptr}
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

  m_object =device->createImage(info());

  vk::MemoryRequirements memRequirements = device->getImageMemoryRequirements(get());
  m_memory = Memory{device, memRequirements, mem_flags};
  m_memory.bindImage(*this, 0);

  if ((usage ^ vk::ImageUsageFlagBits::eTransferSrc) &&
      (usage ^ vk::ImageUsageFlagBits::eTransferDst) &&
      (usage ^ (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc))) {
    createView();
  }
}


Image::Image(Device const& device, pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout)
 :Image{}
{
  m_device = &device;
 
  vk::DeviceSize imageSize = pixel_input .width * pixel_input.height * num_channels(pixel_input.format); 

  // create staging buffer and upload data
  Image img_staging{device, pixel_input.width, pixel_input.height, pixel_input.format, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  // create storage buffer and transfer data
  Image img_store{device, pixel_input.width, pixel_input.height, pixel_input.format, vk::ImageTiling::eOptimal, usage | vk::ImageUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal};

  img_staging.setData(pixel_input.ptr(), imageSize);

  img_staging.transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);
  img_store.transitionToLayout(vk::ImageLayout::eTransferDstOptimal);
  device.copyImage(img_staging, img_store, pixel_input.width, pixel_input.height);

  img_store.transitionToLayout(layout);

  // assign filled buffer to this
  swap(img_store);

  createView();
}

void Image::setData(void const* data, vk::DeviceSize const& size) {
  m_memory.setData(data, size);
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

 void Image::swap(Image& dev) {
  WrapperImage::swap(dev);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_device, dev.m_device);
  std::swap(m_view, dev.m_view);
 }