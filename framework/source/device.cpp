#include "device.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include "device.hpp"
#include "swap_chain.hpp"
#include <vector>
#include <set>

Device::Device()
 :Wrapper<vk::Device, vk::DeviceCreateInfo>{}
 ,m_queue_graphics{VK_NULL_HANDLE}
 ,m_queue_present{VK_NULL_HANDLE}
 ,m_index_graphics{-1}
 ,m_index_present{-1}
 ,m_extensions{}
 ,m_command_pool{VK_NULL_HANDLE}
{}

void Device::create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions) {
  m_phys_device = phys_dev;
  m_index_graphics = graphics;
  m_index_present = present;
  m_extensions = deviceExtensions;

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies = {graphics, present};

  float queuePriority = 1.0f;
  for (int queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }
  
  vk::PhysicalDeviceFeatures deviceFeatures{};
  vk::DeviceCreateInfo createInfo{};
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  vk::DeviceCreateInfo a{createInfo};
  phys_dev.createDevice(&a, nullptr, &get());

  m_queue_graphics = get().getQueue(graphics, 0);
  m_queue_present = get().getQueue(present, 0);
  // throw std::exception();

  createCommandPool();
}

void Device::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.queueFamilyIndex = m_index_graphics;

  m_command_pool = get().createCommandPool(poolInfo);
}

SwapChain Device::createSwapChain(vk::SurfaceKHR const& surf, vk::Extent2D const& extend) const {
  SwapChain chain{};
  chain.create(*this, m_phys_device, surf, extend);
  return chain; 
}

Device::Device(Device && dev)
 :Device{}
 {
  swap(dev);
 }

void Device::destroy() {
  get().destroyCommandPool(m_command_pool);
  get().destroy();
}

 Device& Device::operator=(Device&& dev) {
  swap(dev);
  return *this;
 }

 void Device::swap(Device& dev) {
  // std::swap(m_device, dev.m_device);
  std::swap(get(), dev.get());
  std::swap(m_phys_device, dev.m_phys_device);
  std::swap(m_queue_graphics, dev.m_queue_graphics);
  std::swap(m_queue_present, dev.m_queue_present);
  std::swap(m_index_graphics, dev.m_index_graphics);
  std::swap(m_index_present, dev.m_index_present);
  std::swap(m_extensions, dev.m_extensions);
  std::swap(m_command_pool, dev.m_command_pool);
 }

 vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
 }

vk::CommandPool const& Device::pool() const {
  return m_command_pool;
}

vk::Queue const& Device::queueGraphics() const {
  return m_queue_graphics;
}

vk::Queue const& Device::queuePresent() const {
  return m_queue_present;
}

int const& Device::indexGraphics() const {
  return m_index_graphics;
}

int const& Device::indexPresent() const {
  return m_index_present;
}