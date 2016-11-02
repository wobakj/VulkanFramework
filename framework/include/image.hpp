#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrapper.hpp"
#include "device.hpp"
#include "pixel_data.hpp"
#include "memory.hpp"

#include <vulkan/vulkan.hpp>

vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);

using WrapperImage = Wrapper<vk::Image, vk::ImageCreateInfo>;
class Image : public WrapperImage {
 public:
  
  Image();
  Image(Device const& device, std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags); 
  Image(Device const& device, pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout); 

  // Image(Device const& dev, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties);
  // Image(Device const& device, void* data, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage);
  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  void setData(void const* data, vk::DeviceSize const& size);
  void transitionToLayout(vk::ImageLayout const& newLayout);

  void swap(Image& dev);
  
  vk::Format const& format() const;
  vk::ImageView const& view() const;
  void writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::Sampler const& sampler) const;

 private:
  void destroy() override;
  void createView();

  Memory m_memory;
  Device const* m_device;
  vk::ImageView m_view;
};

#endif