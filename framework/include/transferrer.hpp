#ifndef TRANSFERRER_HPP
#define TRANSFERRER_HPP

#include "wrap/command_buffer.hpp"

#include "allocator_static.hpp"
#include <vulkan/vulkan.hpp>

#include <mutex>
#include <memory>

class Buffer;
class BufferRegion;
class Memory;
class Device;
class BufferView;
class BackedImage;
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
  void uploadBufferData(void const* data_ptr, BufferRegion& buffer);
  void copyBuffer(BufferRegion const& srcBuffer, BufferRegion const& dstBuffer) const;

  // image functions
  void uploadImageData(void const* data_ptr, vk::DeviceSize data_size, BackedImage& image, vk::ImageLayout const& newLayout);
  void copyBufferToImage(Buffer const& srcBuffer, BackedImage& dstImage, uint32_t width, uint32_t height, uint32_t depth = 1) const;
  void copyBufferToImage(Buffer const& srcBuffer, ImageView& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;
  void copyImageToBuffer(Buffer const& srcBuffer, ImageView const& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;

  void copyImage(BackedImage const& srcImage, BackedImage& dstImage, uint32_t width, uint32_t height) const;
  void transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(BackedImage& img, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(BackedImage& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const;
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