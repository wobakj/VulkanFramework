#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrap/image_view.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

bool is_depth(vk::Format const& format);
bool has_stencil(vk::Format const& format);

vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features);

class Image {
 public:
  
  Image();
  Image(Device const& device, vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage); 
  virtual ~Image() {};

  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  void swap(Image& dev);

  virtual ImageRange range() const = 0;
  virtual operator ImageRange() const = 0;

  virtual ImageLayers layers(uint32_t level = 0) const = 0;
  virtual operator ImageLayers() const = 0;

  virtual vk::Format const& format() const;
  virtual ImageView const& view() const;
  virtual vk::Extent3D const& extent() const;

  virtual vk::ImageCreateInfo const& info() const = 0;
 
 protected:
  // virtual vk::ImageCreateInfo& info() = 0;
  virtual vk::Image const& obj() const = 0;
  virtual vk::Device const& device() const = 0;
  virtual void createView();

  ImageView m_view;
  friend class Transferrer;
  friend class ImageView;
};

#endif