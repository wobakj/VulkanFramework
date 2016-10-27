#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <debug_reporter.hpp>
#include <swap_chain.hpp>
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <set>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

std::vector<const char*> getRequiredExtensions(bool enableValidationLayers) {
  std::vector<const char*> extensions;

  unsigned int glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  for (unsigned int i = 0; i < glfwExtensionCount; i++) {
      extensions.push_back(glfwExtensions[i]);
  }

  if (enableValidationLayers) {
      extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  return extensions;
}

bool checkValidationLayerSupport(std::vector<std::string> const& validationLayers) {
  auto availableLayers = vk::enumerateInstanceLayerProperties();

  for (auto const& layerName : validationLayers) {
    bool layerFound = false;
    for (const auto& layerProperties : availableLayers) {
      std::cout << layerProperties.layerName << std::endl;
      if (layerName == layerProperties.layerName) {
        layerFound = true;
        std::cout << "found" << std::endl;
        break;
      }
    }

    if (!layerFound) {
        return false;
    }
  }
  return true;
}

bool checkDeviceExtensionSupport(vk::PhysicalDevice device, std::vector<const char*> const& deviceExtensions) {
    auto availableExtensions = device.enumerateDeviceExtensionProperties(nullptr);

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool isDeviceSuitable(vk::PhysicalDevice const& device, vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions) {
  // VkPhysicalDeviceProperties deviceProperties;
  // vkGetPhysicalDeviceProperties(device, &deviceProperties);
  // std::cout << "Device name " << deviceProperties.deviceName << std::endl;
  // VkPhysicalDeviceFeatures deviceFeatures;
  // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  // return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
  //        deviceFeatures.geometryShader;
  QueueFamilyIndices indices = findQueueFamilies(device, surface);

  bool extensionsSupported = checkDeviceExtensionSupport(device, deviceExtensions);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
      SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
      swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

class Instance {
 public:
  // Instance()
  //  :m_instance{VK_NULL_HANDLE}
  //  ,m_layers{}
  // {}

  Instance(bool validate = true)
   :m_instance{VK_NULL_HANDLE}
   ,m_validate{validate}
   ,m_layers{validate ? std::vector<std::string>{"VK_LAYER_LUNARG_standard_validation"} : std::vector<std::string>{}}
  {}

  void create() {
    vk::ApplicationInfo appInfo = {};
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    vk::InstanceCreateInfo createInfo = {};
    createInfo.pApplicationInfo = &appInfo;

    std::vector<char const*> layers(m_layers.size());
    if (m_validate) {
      if (!checkValidationLayerSupport(m_layers)) {
        throw std::runtime_error("validation layers requested, but not available!");
      }
      else {
        unsigned i = 0;
        for(auto const& layer : m_layers) {
          layers[i] = layer.c_str();
          ++i;
        }
        createInfo.enabledLayerCount = uint32_t(m_layers.size());
        createInfo.ppEnabledLayerNames = layers.data();
      }
    }
     else {
      createInfo.enabledLayerCount = 0;
    }

    auto extensions = getRequiredExtensions(m_validate);
    createInfo.enabledExtensionCount = uint32_t(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    m_instance = vk::createInstance(createInfo, nullptr);
    // create and attach debug callback
    if(m_validate) {
      m_debug_report.attach(m_instance);
    }
  }

  // Device createDevice(std::vector<const char*> extensions, vk::Surface const& surf) {

vk::PhysicalDevice pickPhysicalDevice(vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions) {
  vk::PhysicalDevice phys_device{};
  auto devices = m_instance.enumeratePhysicalDevices();
  std::cout << "devices:" << std::endl;
  for (const auto& device : devices) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    std::cout << "  " << deviceProperties.deviceName << std::endl;
  }
  for (const auto& device : devices) {
    if (isDeviceSuitable(device, surface, deviceExtensions)) {
        phys_device = device;
        break;
    }
  }
  if (!phys_device) {
      throw std::runtime_error("failed to find a suitable GPU!");
  }
  return phys_device;
}

Device createLogicalDevice(vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions) {

  auto const& phys_device = pickPhysicalDevice(surface, deviceExtensions);
  QueueFamilyIndices indices = findQueueFamilies(phys_device, surface);
  Device device{};
  device.create(phys_device, indices.graphicsFamily, indices.presentFamily, deviceExtensions);
  return device;
}


  // }


  ~Instance() {
    destroy();
  }

  void destroy() { 
    if (m_instance) {
      m_instance.destroy();
    }
  }

  operator vk::Instance() const {
      return m_instance;
  }

  vk::Instance const& get() const {
      return m_instance;
  }
  vk::Instance& get() {
      return m_instance;
  }

  vk::Instance* operator->() {
    return &m_instance;
  }

  vk::Instance const* operator->() const {
    return &m_instance;
  }

 private:
  vk::Instance m_instance;
  bool m_validate;
  std::vector<std::string> m_layers;
  DebugReporter m_debug_report;
};

#endif