#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <set>

class SwapChain;
class Buffer;

using WrapDevice = Wrapper<vk::Device, vk::DeviceCreateInfo>;
class Device : public WrapDevice {
 public:
  
  Device();

  Device(Device const&) = delete;
  Device& operator=(Device const&) = delete;

  void create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions);
  
  SwapChain createSwapChain(vk::SurfaceKHR const& surf, vk::Extent2D const& extend) const;

  Device(Device && dev);

  Device& operator=(Device&& dev);

  void swap(Device& dev);
  // buffer functions
  Buffer createBuffer(vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties) const;
  Buffer createBuffer(void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) const;

  void copyBuffer(VkBuffer const& srcBuffer, VkBuffer const& dstBuffer, VkDeviceSize const& size) const;

  // getter
  vk::PhysicalDevice const& physical() const;

  vk::Queue const& queueGraphics() const;

  vk::Queue const& queuePresent() const;

  int const& indexGraphics() const;

  int const& indexPresent() const;

  vk::CommandPool const& pool() const;

 private:
  void destroy() override;
  void createCommandPool();

  vk::PhysicalDevice m_phys_device;
  vk::Queue m_queue_graphics;
  vk::Queue m_queue_present;
  int m_index_graphics;
  int m_index_present;
  std::vector<const char*> m_extensions;
  vk::CommandPool m_command_pool;
};

#endif