#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrapper.hpp"
#include "device.hpp"

#include <vulkan/vulkan.hpp>

class SwapChain;

using WrapperImage = Wrapper<vk::Image, vk::ImageCreateInfo>;
class Image : public WrapperImage {
 public:
  
  Image();
  Image(Device const& device, std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags); 

  // Image(Device const& dev, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties);
  // Image(Device const& device, void* data, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage);
  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  void setData(void const* data, vk::DeviceSize const& size);

  void swap(Image& dev);
  
  vk::DescriptorImageInfo const& descriptorInfo() const;
  void writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const;

 private:
  void destroy() override;

  vk::DeviceMemory m_memory;
  vk::MemoryAllocateInfo m_mem_info;
  Device const* m_device;
  vk::DescriptorImageInfo m_desc_info;
};

#endif