#include "wrap/image_res.hpp"

#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"
#include "wrap/memory.hpp"

#include <iostream>

BackedImage::BackedImage()
 :ResourceImage{}
 ,Image{}
 // ,m_view{}
{}

BackedImage::BackedImage(BackedImage && dev)
 :BackedImage{}
{
  swap(dev);
}

BackedImage::BackedImage(Device const& device,  vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage) 
 :BackedImage{}
{  
  m_device = device.get();

  if (extent.height > 1) {
    if (extent.depth > 1) {
      m_info.imageType = vk::ImageType::e3D;
    }
    else {
      m_info.imageType = vk::ImageType::e2D;
    }
  }
  else {
    m_info.imageType = vk::ImageType::e1D;
  }

  m_info.extent = extent;
  m_info.mipLevels = 1;
  m_info.arrayLayers = 1;
  m_info.format = format;
  m_info.tiling = tiling;
  // if image is linear, its probably for data upload
  if (tiling == vk::ImageTiling::eLinear) {
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
}

BackedImage::~BackedImage() {
  cleanup();
}

void BackedImage::bindTo(vk::DeviceMemory const& mem, vk::DeviceSize const& offst) {
  ResourceImage::bindTo(mem, offst);
  m_device.bindImageMemory(get(), mem, offst);

  if ((info().usage ^ vk::ImageUsageFlagBits::eTransferSrc) &&
      (info().usage ^ vk::ImageUsageFlagBits::eTransferDst) &&
      (info().usage ^ (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc))) {
    createView();
  }
}

void BackedImage::destroy() {
  m_device.destroyImage(get());
}

BackedImage& BackedImage::operator=(BackedImage&& dev) {
  swap(dev);
  return *this;
}

void BackedImage::swap(BackedImage& dev) {
  ResourceImage::swap(dev);
  std::swap(m_view, dev.m_view);
}

vk::ImageCreateInfo const& BackedImage::info() const {
  return ResourceImage::info();
}

vk::Image const& BackedImage::obj() const {
  return get();
}

vk::Device const& BackedImage::device() const {
  return m_device;
}

vk::MemoryRequirements BackedImage::requirements() const {
  return m_device.getImageMemoryRequirements(get());
}

BackedImage::operator ImageRange() const {
  return range();
}

ImageRange BackedImage::range() const {
  vk::ImageSubresourceRange range{img_to_range(info())};
  return ImageRange{get(), range, extent()};
}

BackedImage::operator ImageLayers() const {
  return layers();
}

ImageLayers BackedImage::layers(uint32_t level) const {
  ImageRange range{get(), img_to_range(info()), extent()};
  return range.layers(level);
}
