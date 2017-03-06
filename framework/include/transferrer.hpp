#ifndef TRANSFERRER_HPP
#define TRANSFERRER_HPP

#include <vulkan/vulkan.hpp>

#include <mutex>
#include <memory>

class Buffer;
class Memory;
class Device;
class BufferView;
class Image;
class CommandPool;

class Transferrer {
 public:
  Transferrer();
  Transferrer(Device const& device, CommandPool& pool);
  Transferrer(Transferrer && dev);
  Transferrer(Transferrer const&) = delete;

  ~Transferrer();
  
  Transferrer& operator=(Transferrer&& dev);
  Transferrer& operator=(Transferrer const&) = delete;

  void swap(Transferrer& dev);

  // buffer functions
  void uploadBufferData(void const* data_ptr, BufferView& buffer);
  void uploadBufferData(void const* data_ptr, Buffer& buffer, vk::DeviceSize const& offset = 0);
  void uploadBufferData(void const* data_ptr, vk::DeviceSize const& size, Buffer& buffer, vk::DeviceSize const& offset = 0);
  void copyBuffer(vk::Buffer const& srcBuffer, vk::Buffer const& dstBuffer, vk::DeviceSize const& size, vk::DeviceSize const& src_offset = 0, vk::DeviceSize const& dst_offset = 0) const;

  // image functions
  void uploadImageData(void const* data_ptr, Image& image);
  void copyBufferToImage(Buffer const& srcBuffer, Image& dstImage, uint32_t width, uint32_t height) const;
  void copyImage(Image const& srcImage, Image& dstImage, uint32_t width, uint32_t height) const;
  void transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& newLayout) const;
  void transitionToLayout(Image& img, vk::ImageLayout const& newLayout) const;

  // helper functions to create commandbuffer for staging an formating
  vk::CommandBuffer const& beginSingleTimeCommands() const;
  void endSingleTimeCommands() const;

  void deallocate();

  Device const& device() const;

 private:
  void adjustStagingPool(vk::DeviceSize const& size);
  // void destroy() override;

  Device const* m_device;
  CommandPool const* m_pool;
  vk::CommandBuffer m_command_buffer_help;
  std::unique_ptr<Buffer> m_buffer_stage;
  std::unique_ptr<Memory> m_memory_stage;
  mutable std::mutex m_mutex_single_command;
  mutable std::mutex m_mutex_staging;
};

#endif