#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrapper.hpp"
#include "pixel_data.hpp"
#include "memory.hpp"

#include <vulkan/vulkan.hpp>

class Device;

bool is_depth(vk::Format const& format);
bool has_stencil(vk::Format const& format);

vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info);
vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level = 0);

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout);
vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);
vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features);

using WrapperImage = Wrapper<vk::Image, vk::ImageCreateInfo>;
class Image : public WrapperImage {
 public:
  
  Image();
  Image(Device const& device, std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags); 
  Image(Device const& device, pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout); 
  ~Image();

  // Image(Device const& dev, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties);
  // Image(Device const& device, void* data, vk::DeviceSize const& size, vk::ImageUsageFlags const& usage);
  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  void setData(void const* data, vk::DeviceSize const& size);
  void transitionToLayout(vk::ImageLayout const& newLayout);
  void bindTo(Memory& memory);

  void swap(Image& dev);

  vk::DeviceSize size() const;
  vk::MemoryRequirements requirements() const;
  vk::MemoryPropertyFlags const& memFlags() const;
 
  vk::ImageLayout const& layout() const;
  vk::AttachmentDescription toAttachment(bool clear = true) const;
  vk::Format const& format() const;
  vk::ImageView const& view() const;
  // write as combined sampler
  void writeToSet(vk::DescriptorSet& set, std::uint32_t binding, vk::Sampler const& sampler) const;
  // write as input attachment
  void writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const;

 private:
  void destroy() override;
  void createView();

  vk::DeviceMemory m_memory;
  Device const* m_device;
  vk::ImageView m_view;
  vk::DeviceSize m_offset;
  vk::MemoryPropertyFlags m_flags_mem;
};

#endif