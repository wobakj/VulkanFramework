#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include "deleter.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
      return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

inline QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
  QueueFamilyIndices indices;

  auto queueFamilies = device.getQueueFamilyProperties();
  
  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphicsFamily = i;
    }
    
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (queueFamily.queueCount > 0 && presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }
  return indices;
}


 inline  SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
  }

  inline VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      VkExtent2D actualExtent = extent;

      actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
      actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
      
      return actualExtent;
    }
  }

  inline VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
      return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
          return availableFormat;
      }
    }

    return availableFormats[0];
  }

  inline vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes) {
      for (const auto& availablePresentMode : availablePresentModes) {
          if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
              return availablePresentMode;
          }
      }

      return vk::PresentModeKHR::eFifo;
  }


inline std::vector<Deleter<VkImageView>> createImageViews(vk::Device device, std::vector<vk::Image> const& images, vk::Format const& format) {
  std::vector<Deleter<VkImageView>> views(images.size(), Deleter<VkImageView>{device, vkDestroyImageView});
  for (uint32_t i = 0; i < images.size(); i++) {
    vk::ImageViewCreateInfo createInfo = {};
    createInfo.image = images[i];

    createInfo.viewType = vk::ImageViewType::e2D;
    createInfo.format = format;

    createInfo.components.r = vk::ComponentSwizzle::eIdentity;
    createInfo.components.g = vk::ComponentSwizzle::eIdentity;
    createInfo.components.b = vk::ComponentSwizzle::eIdentity;
    createInfo.components.a = vk::ComponentSwizzle::eIdentity;

    createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    vk::ImageView view = device.createImageView(createInfo, nullptr);
    views[i] = view;
  }
  return views;
}

class SwapChain {
 public:
  
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;

  SwapChain(vk::Device dev = VK_NULL_HANDLE, vk::PhysicalDevice phys_dev = VK_NULL_HANDLE, vk::SurfaceKHR surface = VK_NULL_HANDLE, VkExtent2D extend = {0,0})
   :m_swapchain{VK_NULL_HANDLE}
   ,m_device{dev}
   ,m_format_swap{vk::Format::eUndefined}
   ,m_extent_swap{extend}
   ,m_images_swap{}
   ,m_views_swap{}
  {}
 
  SwapChain(SwapChain && chain)
   :SwapChain{}
   {
    swap(chain);
   }

   SwapChain& operator=(SwapChain&& chain) {
    swap(chain);
    return *this;
   }

   void swap(SwapChain& chain) {
    std::swap(m_swapchain, chain.m_swapchain);
    std::swap(m_device, chain.m_device);
    std::swap(m_format_swap, chain.m_format_swap);
    std::swap(m_extent_swap, chain.m_extent_swap);
    std::swap(m_images_swap, chain.m_images_swap);
    std::swap(m_views_swap, chain.m_views_swap);
   }

  void create(vk::Device dev, vk::PhysicalDevice phys_device, vk::SurfaceKHR surface, VkExtent2D extend) {
    m_device = dev;
    m_extent_swap = extend;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(phys_device, surface);

    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    m_extent_swap = chooseSwapExtent(swapChainSupport.capabilities, m_extent_swap);
    m_format_swap = surfaceFormat.format;

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo = {};
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = m_extent_swap;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(phys_device, surface);
    uint32_t queueFamilyIndices[] = {(uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    m_swapchain = m_device.createSwapchainKHR(createInfo, nullptr);
    m_images_swap = m_device.getSwapchainImagesKHR(m_swapchain);
    m_views_swap = createImageViews(m_device, m_images_swap, m_format_swap);
  }


  ~SwapChain() {
    destroy();
  }

  void destroy() { 
    if (m_swapchain) {
      m_device.destroySwapchainKHR(m_swapchain);
    }
  }

  operator vk::SwapchainKHR() const {
      return m_swapchain;
  }

  vk::SwapchainKHR const& get() const {
      return m_swapchain;
  }
  vk::SwapchainKHR& get() {
      return m_swapchain;
  }

  vk::SwapchainKHR* operator->() {
    return &m_swapchain;
  }

  vk::SwapchainKHR const* operator->() const {
    return &m_swapchain;
  }

  std::vector<Deleter<VkImageView>> const& views() const {
    return m_views_swap;
  }

  std::vector<vk::Image> const& images() const {
    return m_images_swap;
  }

  vk::Format format() const {
    return m_format_swap;
  }

  vk::Extent2D const& extend() const {
    return m_extent_swap;
  }
 private:
  vk::SwapchainKHR m_swapchain;
  vk::Device m_device;
  vk::Format m_format_swap;
  vk::Extent2D m_extent_swap;
  std::vector<vk::Image> m_images_swap;
  std::vector<Deleter<VkImageView>> m_views_swap;
};

#endif