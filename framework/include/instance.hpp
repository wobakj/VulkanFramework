#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <debug_reporter.hpp>
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

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