#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "wrap/wrapper.hpp"
#include "wrap/memory.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <mutex>
#include <memory>

class SwapChain;
class Buffer;
class BufferView;
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
  
  Device(Device && dev);

  Device& operator=(Device&& dev);

  void swap(Device& dev);
  // get memory types for usages
  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) const;
  uint32_t suitableMemoryTypes(vk::Format const& format, vk::ImageTiling const& tiling) const;
  uint32_t suitableMemoryTypes(vk::BufferUsageFlags const& usage) const;
  uint32_t suitableMemoryType(vk::Format const& format, vk::ImageTiling const& tiling, vk::MemoryPropertyFlags const& properties) const;
  uint32_t suitableMemoryType(vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& properties) const;

  // getter
  vk::PhysicalDevice const& physical() const;

  vk::Queue const& getQueue(std::string const& name) const;
  uint32_t getQueueIndex(std::string const& name) const;

  std::vector<uint32_t> ownerIndices() const;

 private:
  void destroy() override;

  vk::PhysicalDevice m_phys_device;
  std::map<std::string, uint32_t> m_queue_indices;
  std::map<std::string, vk::Queue> m_queues;
  std::map<std::string, vk::CommandPool> m_pools;
  std::vector<const char*> m_extensions;
};

#endif