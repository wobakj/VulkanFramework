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

  std::string string_flag;
  bool throwing = false; 
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    string_flag += "Error ";
    throwing = true;
  }
  if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    string_flag += "Warning ";
  }
  if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    string_flag += "Performance ";
  }
  if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    string_flag += "Info ";
  }
  if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    string_flag += "Debug ";
  }
  std::cerr << string_flag << "- " << msg << std::endl;
  if (throwing) {
    assert(0);
    throw std::runtime_error{"msg"};
  }
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
   ,m_callback()
  {}

  void attach(vk::Instance const& inst) {
    destroy();

    m_instance = &inst;
    vk::DebugReportCallbackCreateInfoEXT createInfo = {};
    createInfo.flags = vk::DebugReportFlagBitsEXT::eError
                     | vk::DebugReportFlagBitsEXT::eWarning 
                     | vk::DebugReportFlagBitsEXT::ePerformanceWarning 
                     // | vk::DebugReportFlagBitsEXT::eDebug 
                     // | vk::DebugReportFlagBitsEXT::eInformation
                     ;
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
      m_callback = vk::DebugReportCallbackEXT{};
    }
  }

 private:
  vk::Instance const* m_instance;
  vk::DebugReportCallbackEXT m_callback; 
};

#endif