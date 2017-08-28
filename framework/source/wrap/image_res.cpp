#include "wrap/image_res.hpp"

#include "wrap/render_pass.hpp"
#include "wrap/device.hpp"
#include "wrap/memory.hpp"

#include <iostream>

ImageRes::ImageRes()
 :ResourceImage{}
 ,Image{}
 // ,m_view{}
{}

ImageRes::ImageRes(ImageRes && dev)
 :ImageRes{}
{
  swap(dev);
}

ImageRes::ImageRes(Device const& device,  vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage) 
 :ImageRes{}
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

ImageRes::~ImageRes() {
  cleanup();
}

void ImageRes::bindTo(vk::DeviceMemory const& mem, vk::DeviceSize const& offst) {
  ResourceImage::bindTo(mem, offst);
  m_device.bindImageMemory(get(), mem, offst);

  if ((info().usage ^ vk::ImageUsageFlagBits::eTransferSrc) &&
      (info().usage ^ vk::ImageUsageFlagBits::eTransferDst) &&
      (info().usage ^ (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc))) {
    createView();
  }
}

void ImageRes::destroy() {
  m_device.destroyImage(get());
}

ImageRes& ImageRes::operator=(ImageRes&& dev) {
  swap(dev);
  return *this;
}

void ImageRes::swap(ImageRes& dev) {
  ResourceImage::swap(dev);
  std::swap(m_view, dev.m_view);
}

vk::ImageCreateInfo const& ImageRes::info() const {
  return ResourceImage::info();
}

vk::Image const& ImageRes::obj() const {
  return get();
}

vk::Device const& ImageRes::device() const {
  return m_device;
}

vk::MemoryRequirements ImageRes::requirements() const {
  return m_device.getImageMemoryRequirements(get());
}