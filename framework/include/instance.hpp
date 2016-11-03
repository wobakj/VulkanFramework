#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include "debug_reporter.hpp"
#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>

class Device;

std::vector<const char*> getRequiredExtensions(bool enableValidationLayers);

// bool checkValidationLayerSupport(std::vector<std::string> const& validationLayers);
// bool checkDeviceExtensionSupport(vk::PhysicalDevice device, std::vector<const char*> const& deviceExtensions);
// bool isDeviceSuitable(vk::PhysicalDevice const& device, vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions);

class Instance : public Wrapper<vk::Instance, vk::InstanceCreateInfo> {
 public:
  Instance(bool validate = true);
  Instance(Instance const&) = delete;

  Instance& operator=(Instance const&) = delete;

  void create();

  vk::PhysicalDevice pickPhysicalDevice(vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions);
  Device createLogicalDevice(vk::SurfaceKHR const& surface, std::vector<const char*> const& deviceExtensions);

 private:
  void destroy() override;
  
  bool m_validate;
  std::vector<std::string> m_layers;
  DebugReporter m_debug_report;
};

#endif