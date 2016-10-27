#ifndef DEVICE_HPP
#define DEVICE_HPP

// #include "swap_chain.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <set>

class SwapChain;

class Device {
 public:
  
  Device();

  Device(Device const&) = delete;
  Device& operator=(Device const&) = delete;

  void create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions);
  
  SwapChain createSwapChain() const;

  Device(Device && dev);

   Device& operator=(Device&& dev);

   void swap(Device& dev);

   vk::PhysicalDevice const& physical() const;

   void set(vk::Device const& dev);

  ~Device();

  void destroy();

  operator vk::Device() const;

  vk::Device const& get() const;

  vk::Device& get();

  vk::Device* operator->();

  vk::Device const* operator->() const;

  vk::Queue const& queueGraphics() const;

  vk::Queue const& queuePresent() const;

  int const& indexGraphics() const;

  int const& indexPresent() const;

  vk::Device m_device;
  // vk::Instance m_instance;

 private:
  vk::PhysicalDevice m_phys_device;
  vk::Queue m_queue_graphics;
  vk::Queue m_queue_present;
  int m_index_graphics;
  int m_index_present;
  std::vector<const char*> m_extensions;
};

#endif