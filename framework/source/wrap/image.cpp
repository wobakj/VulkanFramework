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

Image& Image::operator=(Image&& dev) {
  swap(dev);
  return *this;
}

ImageView const& Image::view() const {
  return m_view;
}

vk::Format const& Image::format() const {
  return info().format;
}

vk::Extent3D const& Image::extent() const {
  return info().extent;
}

void Image::createView() {
  // auto view_info = img_to_view(obj(), info()); 
  m_view = ImageView{*this};  
}

void Image::swap(Image& dev) {
  std::swap(m_view, dev.m_view);
 }
