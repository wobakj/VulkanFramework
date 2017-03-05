#ifndef IMAGE_HPP
#define IMAGE_HPP

#include "wrap/memory_resource.hpp"
#include "wrap/pixel_data.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Memory;

bool is_depth(vk::Format const& format);
bool has_stencil(vk::Format const& format);

vk::ImageSubresourceRange img_to_resource_range(vk::ImageCreateInfo const& img_info);
vk::ImageSubresourceLayers img_to_resource_layer(vk::ImageCreateInfo const img_info, unsigned mip_level = 0);

vk::AccessFlags layout_to_access(vk::ImageLayout const& layout);
vk::ImageViewCreateInfo img_to_view(vk::Image const& image, vk::ImageCreateInfo const& img_info);
vk::Format findSupportedFormat(vk::PhysicalDevice const& physicalDevice, std::vector<vk::Format> const& candidates, vk::ImageTiling const& tiling, vk::FormatFeatureFlags const& features);

using ResourceImage = MemoryResource<vk::Image, vk::ImageCreateInfo>;
class Image : public ResourceImage {
 public:
  
  Image();
  Image(Device const& device, vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage); 
  ~Image();

  Image(Image && dev);
  Image(Image const&) = delete;
  
  Image& operator=(Image const&) = delete;
  Image& operator=(Image&& dev);

  void layoutTransitionCommand(vk::CommandBuffer& buffer, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new);
  // TODO: correct memory type matching
  void bindTo(Memory& memory) override;
  void bindTo(Memory& memory, vk::DeviceSize const& offset) override;

  void swap(Image& dev);

  vk::ImageLayout const& layout() const;
  vk::AttachmentDescription toAttachment(bool clear = true) const;
  vk::Format const& format() const;
  vk::ImageView const& view() const;
  vk::Extent3D const& extent() const;
  // write as combined sampler
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::Sampler const& sampler, uint32_t index = 0) const;
  // write as input attachment
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;
  vk::MemoryRequirements requirements() const;

 private:
  void destroy() override;
  void createView();

  vk::ImageView m_view;

  friend class Transferrer;
};

#endif