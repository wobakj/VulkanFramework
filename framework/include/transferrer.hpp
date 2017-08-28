#ifndef TRANSFERRER_HPP
#define TRANSFERRER_HPP

#include "wrap/command_buffer.hpp"

#include "allocator_static.hpp"
#include <vulkan/vulkan.hpp>

#include <mutex>
#include <memory>

class Buffer;
class Memory;
class Device;
class BufferView;
class ImageRes;
class ImageView;
class CommandPool;
class CommandBuffer;

class Transferrer {
 public:
  Transferrer();
  Transferrer(CommandPool& pool);
  Transferrer(Transferrer && dev);
  Transferrer(Transferrer const&) = delete;

  Transferrer& operator=(Transferrer&& dev);
  Transferrer& operator=(Transferrer const&) = delete;

  void swap(Transferrer& dev);

  // buffer functions
  void uploadBufferData(void const* data_ptr, BufferView& buffer);
  void uploadBufferData(void const* data_ptr, Buffer& buffer, vk::DeviceSize const& offset = 0);
  void uploadBufferData(void const* data_ptr, vk::DeviceSize const& size, Buffer& buffer, vk::DeviceSize const& offset = 0);
  void copyBuffer(vk::Buffer const& srcBuffer, vk::Buffer const& dstBuffer, vk::DeviceSize const& size, vk::DeviceSize const& src_offset = 0, vk::DeviceSize const& dst_offset = 0) const;

  // image functions
  void uploadImageData(void const* data_ptr, ImageRes& image);
  void uploadImageData(void const* data_ptr, ImageRes& image, vk::ImageLayout const& newLayout);
  void copyBufferToImage(Buffer const& srcBuffer, ImageRes& dstImage, uint32_t width, uint32_t height, uint32_t depth = 1) const;
  void copyBufferToImage(Buffer const& srcBuffer, ImageView& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;
  void copyImageToBuffer(Buffer const& srcBuffer, ImageView const& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;

  void copyImage(ImageRes const& srcImage, ImageRes& dstImage, uint32_t width, uint32_t height) const;
  void transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(ImageRes& img, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(ImageRes& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(ImageView const& view, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const;

  // helper functions to create commandbuffer for staging an formating
  CommandBuffer const& beginSingleTimeCommands() const;
  void endSingleTimeCommands() const;

  void deallocate();

  Device const& device() const;

 private:
  void adjustStagingPool(vk::DeviceSize const& size);

  Device const* m_device;
  CommandBuffer m_command_buffer_help;
  std::unique_ptr<StaticAllocator> m_allocator_stage;
  std::unique_ptr<Buffer> m_buffer_stage;
  mutable std::mutex m_mutex_single_command;
  mutable std::mutex m_mutex_staging;
};

#endif