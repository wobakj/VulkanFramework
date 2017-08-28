#ifndef IMAGE_RES_HPP
#define IMAGE_RES_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/image.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

using ResourceImage = MemoryResourceT<vk::Image, vk::ImageCreateInfo>;
class ImageRes : public ResourceImage, public Image {
 public:
  
  ImageRes();
  ImageRes(Device const& device, vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage); 
  ~ImageRes();

  ImageRes(ImageRes && dev);
  ImageRes(ImageRes const&) = delete;
  
  ImageRes& operator=(ImageRes const&) = delete;
  ImageRes& operator=(ImageRes&& dev);

  void bindTo(vk::DeviceMemory const& memory, vk::DeviceSize const& offset) override;
  vk::MemoryRequirements requirements() const override;

  virtual res_handle_t handle() const override {
    return res_handle_t{m_object};
  }
  void swap(ImageRes& dev);

  vk::ImageCreateInfo const& info() const override;
 private:
  vk::Image const& obj() const override;
  vk::Device const& device() const override;

  void destroy() override;

  friend class Transferrer;
  friend class ImageView;
};

#endif