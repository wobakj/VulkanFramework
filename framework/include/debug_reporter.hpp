#ifndef DEBUG_REPORT_HPP
#define DEBUG_REPORT_HPP

#include <vulkan/vulkan.hpp>
#include <iostream>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objType,
  uint64_t obj,
  size_t location,
  int32_t code,
  const char* layerPrefix,
  const char* msg,
  void* userData) {

  std::cerr << "validation layer: " << msg << std::endl;
  throw std::runtime_error{"msg"};
  return VK_FALSE;
}

// implement and geta ddresse sof extions functions
inline VkResult vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

inline void vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

class DebugReporter {
 public:
  DebugReporter()
   :m_instance{nullptr}
   ,m_callback{VK_NULL_HANDLE}
  {}

  void attach(vk::Instance const& inst) {
    destroy();

    m_instance = &inst;
    vk::DebugReportCallbackCreateInfoEXT createInfo = {};
    createInfo.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;
    createInfo.pfnCallback = debug_callback;
    m_callback = inst.createDebugReportCallbackEXT(createInfo);
  }

  DebugReporter const& operator=(DebugReporter&& deb)  {
    destroy();
    m_instance = deb.m_instance;
    m_callback = deb.m_callback;

    return *this;
  }

  ~DebugReporter() {
    destroy();
  }
  void destroy() { 
    if (m_callback) {
      m_instance->destroyDebugReportCallbackEXT(m_callback);
      m_callback = VK_NULL_HANDLE;
    }
  }

 private:
  vk::Instance const* m_instance;
  vk::DebugReportCallbackEXT m_callback; 
};

#endif