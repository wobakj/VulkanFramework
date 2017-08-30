#ifndef IMAGE_RES_HPP
#define IMAGE_RES_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/image.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

// an image that is backed with memory
using ResourceImage = MemoryResourceT<vk::Image, vk::ImageCreateInfo>;
class BackedImage : public ResourceImage, public Image {
 public:
  
  BackedImage();
  BackedImage(Device const& device, vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage); 
  ~BackedImage();

  BackedImage(BackedImage && dev);
  BackedImage(BackedImage const&) = delete;
  
  BackedImage& operator=(BackedImage const&) = delete;
  BackedImage& operator=(BackedImage&& dev);

  void bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset) override;
  vk::MemoryRequirements requirements() const override;

  virtual res_handle_t handle() const override {
    return res_handle_t{m_object};
  }
  void swap(BackedImage& dev);

  vk::ImageCreateInfo const& info() const override;

  virtual ImageRange range() const override;
  virtual operator ImageRange() const override;

  virtual ImageLayers layers(uint32_t level) const override;
  virtual operator ImageLayers() const override;

 private:
  vk::Image const& obj() const override;
  vk::Device const& device() const override;

  void destroy() override;

  friend class Transferrer;
  friend class ImageView;
};

#endif