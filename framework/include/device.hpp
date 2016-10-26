#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

class Device {
 public:
  
  Device(vk::Device const& dev, int graphics, int present)
   :m_device{dev}
   ,m_family_graphics{graphics}
   ,m_family_present{present}
  {}

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

 private:
  vk::Device m_device;
  int m_family_graphics;
  int m_family_present;
};

#endif