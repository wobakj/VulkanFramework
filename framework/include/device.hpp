#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <set>

class Device {
 public:
  
  Device()
   :m_device{VK_NULL_HANDLE}
   ,m_family_graphics{-1}
   ,m_family_present{-1}
  {}

  void create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions) {

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
  }

  Device(vk::Device const& dev)
   :m_device{dev}
   {}

   void set(vk::Device const& dev) {
    m_device = dev;
   }

  ~Device() {
    destroy();
  }

  void destroy() { 
    if (m_device) {
      m_device.destroy();
    }
  }

  operator vk::Device() const {
      return m_device;
  }

  vk::Device const& get() const {
      return m_device;
  }
  vk::Device& get() {
      return m_device;
  }

  vk::Device* operator->() {
    return &m_device;
  }

  vk::Device const* operator->() const {
    return &m_device;
  }

  vk::Device m_device;
  // vk::Instance m_instance;
  int m_family_graphics;
  int m_family_present;
  vk::Queue m_queue_graphics;
  vk::Queue m_queue_present;

 private:
};

#endif