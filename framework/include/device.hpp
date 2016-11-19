#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "wrapper.hpp"
#include "memory.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <mutex>

class SwapChain;
class Buffer;
class Image;
struct pixel_data;
class QueueFamilyIndices;

using WrapperDevice = Wrapper<vk::Device, vk::DeviceCreateInfo>;
class Device : public WrapperDevice {
 public:
  
  Device();
  Device(vk::PhysicalDevice const& phys_dev, QueueFamilyIndices const& queues, std::vector<const char*> const& deviceExtensions);
  ~Device();

  Device(Device const&) = delete;
  Device& operator=(Device const&) = delete;

  
  SwapChain createSwapChain(vk::SurfaceKHR const& surf, vk::Extent2D const& extend) const;

  Device(Device && dev);

  Device& operator=(Device&& dev);

  void swap(Device& dev);
  // buffer functions
  Buffer createBuffer(vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties) const;
  Buffer createBuffer(void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) const;
  void copyBuffer(vk::Buffer const srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize const& size) const;

  // image functions
  Image createImage(std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags) const; 
  Image createImage(pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout);
  void uploadImageData(void const* data_ptr, Image& image);

  void copyImage(Image const& srcImage, Image& dstImage, uint32_t width, uint32_t height) const;
  void transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& newLayout) const;

  // helper functions to create commandbuffer for staging an formating
  vk::CommandBuffer const& beginSingleTimeCommands() const;
  void endSingleTimeCommands() const;

  void waitFence(vk::Fence const& fence) const;

  // getter
  vk::PhysicalDevice const& physical() const;

  vk::Queue const& getQueue(std::string const& name) const;
  uint32_t getQueueIndex(std::string const& name) const;

  Memory& memoryPool(std::string const&);
  void allocateMemoryPool(std::string const&, uint32_t type, vk::DeviceSize const& size);
  void reallocateMemoryPool(std::string const&, uint32_t type, vk::DeviceSize const& size);
  void adjustStagingPool(uint32_t type, vk::DeviceSize const& size);

  vk::CommandPool const& pool(std::string const&) const;
  std::vector<vk::CommandBuffer> createCommandBuffers(std::string const& name_pool, vk::CommandBufferLevel const& level = vk::CommandBufferLevel::ePrimary, uint32_t num = 1) const;
  vk::CommandBuffer createCommandBuffer(std::string const& name_pool, vk::CommandBufferLevel const& level = vk::CommandBufferLevel::ePrimary) const;

  std::vector<uint32_t> ownerIndices() const;

 private:
  void destroy() override;
  void createCommandPools();

  vk::PhysicalDevice m_phys_device;
  std::map<std::string, uint32_t> m_queue_indices;
  std::map<std::string, vk::Queue> m_queues;
  std::map<std::string, vk::CommandPool> m_pools;
  std::map<std::string, Memory> m_pools_memory;
  std::vector<const char*> m_extensions;
  vk::CommandBuffer m_command_buffer_help;

  mutable std::mutex m_mutex;
};

#endif