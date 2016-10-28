#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include "deleter.hpp"
#include "wrapper.hpp"

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

class SwapChain : public Wrapper<vk::SwapchainKHR> {
 public:
  
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;

  SwapChain(vk::Device dev = VK_NULL_HANDLE, vk::PhysicalDevice phys_dev = VK_NULL_HANDLE, vk::SurfaceKHR surface = VK_NULL_HANDLE, VkExtent2D extend = {0,0})
   :Wrapper<vk::SwapchainKHR>{}
   ,m_info{}
   ,m_device{dev}
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
    std::swap(get(), chain.get());
    std::swap(m_info, chain.m_info);
    std::swap(m_device, chain.m_device);
    std::swap(m_phys_device, chain.m_phys_device);
    std::swap(m_images_swap, chain.m_images_swap);
    std::swap(m_views_swap, chain.m_views_swap);
   }

  void create(vk::Device dev, vk::PhysicalDevice phys_device, vk::SurfaceKHR surface, VkExtent2D extend) {
    m_device = dev;
    m_info.surface = surface;
    m_phys_device = phys_device;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(phys_device, surface);

    m_info.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    m_info.minImageCount = imageCount;
    m_info.imageFormat = surfaceFormat.format;
    m_info.imageColorSpace = surfaceFormat.colorSpace;
    m_info.imageArrayLayers = 1;
    m_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(m_phys_device, m_info.surface);
    uint32_t queueFamilyIndices[] = {(uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        m_info.imageSharingMode = vk::SharingMode::eConcurrent;
        m_info.queueFamilyIndexCount = 2;
        m_info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        m_info.imageSharingMode = vk::SharingMode::eExclusive;
        m_info.queueFamilyIndexCount = 0; // Optional
        m_info.pQueueFamilyIndices = nullptr; // Optional
    }
    m_info.preTransform = swapChainSupport.capabilities.currentTransform;
    m_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    m_info.clipped = VK_TRUE;

    recreate(extend);
  }

  void recreate(vk::Extent2D const& extend) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_phys_device, m_info.surface);
    m_info.imageExtent = chooseSwapExtent(swapChainSupport.capabilities, extend);

    m_info.oldSwapchain = get();

    auto new_chain = m_device.createSwapchainKHR(m_info, nullptr);
    replace(std::move(new_chain));

    m_images_swap = m_device.getSwapchainImagesKHR(get());
    m_views_swap = createImageViews(m_device, m_images_swap, m_info.imageFormat);
  }

  std::vector<Deleter<VkImageView>> const& views() const {
    return m_views_swap;
  }

  std::vector<vk::Image> const& images() const {
    return m_images_swap;
  }

  vk::Format format() const {
    return m_info.imageFormat;
  }

  vk::Extent2D const& extend() const {
    return m_info.imageExtent;
  }
 private:

  void destroy() override { 
    m_device.destroySwapchainKHR(get());
  }

  vk::SwapchainCreateInfoKHR m_info;
  vk::Device m_device;
  vk::PhysicalDevice m_phys_device;
  std::vector<vk::Image> m_images_swap;
  std::vector<Deleter<VkImageView>> m_views_swap;
};

#endif