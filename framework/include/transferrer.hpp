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
class ImageRange;
class ImageLayers;
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
  void uploadImageData(void const* data_ptr, vk::DeviceSize data_size, ImageLayers const& image, vk::ImageLayout const& newLayout);
  void uploadImageData(void const* data_ptr, vk::DeviceSize data_size, ImageLayers&& image, vk::ImageLayout const& newLayout);

  void copyBufferToImage(BufferRegion const& srcBuffer, ImageLayers const& dstImage, vk::ImageLayout imageLayout) const;
  void copyImageToBuffer(ImageLayers const& dstImage, vk::ImageLayout imageLayout, BufferRegion const& srcBuffer) const;

  void transitionToLayout(ImageRange const& img, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(ImageRange const& img, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout) const;

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