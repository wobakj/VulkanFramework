#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include "wrap/debug_reporter.hpp"
#include "wrap/wrapper.hpp"

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
  Instance();
  Instance(Instance const&) = delete;

  Instance& operator=(Instance const&) = delete;

  void create(bool validate);

  vk::PhysicalDevice pickPhysicalDevice(std::vector<const char*> const& deviceExtensions, vk::SurfaceKHR const& surface);
  Device createLogicalDevice(std::vector<const char*> const& deviceExtensions, vk::SurfaceKHR const& surface = {});

 private:
  void destroy() override;
  
  bool m_validate;
  std::vector<std::string> m_layers;
  DebugReporter m_debug_report;
};

#endif