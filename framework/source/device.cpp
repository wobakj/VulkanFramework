#include "device.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include "device.hpp"
#include "swap_chain.hpp"
#include <vector>
#include <set>

  Device::Device()
   :m_device{VK_NULL_HANDLE}
   ,m_queue_graphics{VK_NULL_HANDLE}
   ,m_queue_present{VK_NULL_HANDLE}
   ,m_index_graphics{-1}
   ,m_index_present{-1}
   ,m_extensions{}
  {}

  void Device::create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions) {
    m_phys_device = phys_dev;
    m_index_graphics = graphics;
    m_index_present = present;
    m_extensions = deviceExtensions;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = {graphics, present};

    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    vk::DeviceCreateInfo a{createInfo};
    phys_dev.createDevice(&a, nullptr, &m_device);

    m_queue_graphics = m_device.getQueue(graphics, 0);
    m_queue_present = m_device.getQueue(present, 0);
    // throw std::exception();
  }

  SwapChain Device::createSwapChain() const {
    SwapChain chain{};
    return chain; 
  }

  Device::Device(Device && dev)
   :Device{}
   {
    swap(dev);
   }

   Device& Device::operator=(Device&& dev) {
    swap(dev);
    return *this;
   }

   void Device::swap(Device& dev) {
    std::swap(m_device, dev.m_device);
    std::swap(m_phys_device, dev.m_phys_device);
    std::swap(m_queue_graphics, dev.m_queue_graphics);
    std::swap(m_queue_present, dev.m_queue_present);
    std::swap(m_index_graphics, dev.m_index_graphics);
    std::swap(m_index_present, dev.m_index_present);
    std::swap(m_extensions, dev.m_extensions);
   }

   vk::PhysicalDevice const& Device::physical() const {
    return m_phys_device;
   }

  Device::~Device() {
    destroy();
  }

  void Device::destroy() { 
    if (m_device) {
      m_device.destroy();
    }
  }

  Device::operator vk::Device() const {
      return m_device;
  }

  vk::Device const& Device::get() const {
      return m_device;
  }
  vk::Device& Device::get() {
      return m_device;
  }

  vk::Device* Device::operator->() {
    return &m_device;
  }

  vk::Device const* Device::operator->() const {
    return &m_device;
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