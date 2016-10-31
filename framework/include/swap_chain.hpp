#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include "deleter.hpp"
#include "wrapper.hpp"
#include "device.hpp"
#include "image.hpp"

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

inline vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass) {
  vk::ImageView attachments[] = {view};

  vk::FramebufferCreateInfo fb_info{};
  fb_info.renderPass = pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = attachments;
  fb_info.width = img_info.extent.width;
  fb_info.height = img_info.extent.height;
  fb_info.layers = img_info.arrayLayers;
  return fb_info;
}

inline vk::ImageCreateInfo chain_to_img(vk::SwapchainCreateInfoKHR const& swap_info) {
  vk::ImageCreateInfo img_info{};
  img_info.imageType = vk::ImageType::e2D;
  img_info.tiling = vk::ImageTiling::eOptimal;
  img_info.mipLevels = 1;
  img_info.format = swap_info.imageFormat;
  img_info.arrayLayers = swap_info.imageArrayLayers;
  img_info.extent = vk::Extent3D{swap_info.imageExtent.width, swap_info.imageExtent.height, 0};
  img_info.usage = swap_info.imageUsage;
  img_info.sharingMode = swap_info.imageSharingMode;
  img_info.queueFamilyIndexCount = swap_info.queueFamilyIndexCount;
  img_info.pQueueFamilyIndices = swap_info.pQueueFamilyIndices;
  return img_info;
}

inline vk::ImageView createImageView(vk::Device const& device, vk::Image const& image, vk::ImageCreateInfo const& img_info) {
  return device.createImageView(img_to_view(image, img_info), nullptr);
}

using WrapperSwap = Wrapper<vk::SwapchainKHR, vk::SwapchainCreateInfoKHR>;
class SwapChain : public WrapperSwap {
 public:
  
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;

  SwapChain()
   :WrapperSwap{}
   ,m_device{nullptr}
   // ,m_images_swap{}
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
    WrapperSwap::swap(chain);
    std::swap(m_device, chain.m_device);
    std::swap(m_phys_device, chain.m_phys_device);
    std::swap(m_views_swap, chain.m_views_swap);
   }

  void create(Device const& dev, vk::PhysicalDevice const& phys_device, vk::SurfaceKHR const& surface, VkExtent2D const& extent) {
    m_device = &dev;
    info().surface = surface;
    m_phys_device = phys_device;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(phys_device, surface);

    info().presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    info().minImageCount = imageCount;
    info().imageFormat = surfaceFormat.format;
    info().imageColorSpace = surfaceFormat.colorSpace;
    info().imageArrayLayers = 1;
    info().imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(m_phys_device, info().surface);
    uint32_t queueFamilyIndices[] = {(uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        info().imageSharingMode = vk::SharingMode::eConcurrent;
        info().queueFamilyIndexCount = 2;
        info().pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        info().imageSharingMode = vk::SharingMode::eExclusive;
        info().queueFamilyIndexCount = 0; // Optional
        info().pQueueFamilyIndices = nullptr; // Optional
    }
    info().preTransform = swapChainSupport.capabilities.currentTransform;
    info().compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info().clipped = VK_TRUE;

    recreate(extent);

  }

  void recreate(vk::Extent2D const& extent) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_phys_device, info().surface);
    info().imageExtent = chooseSwapExtent(swapChainSupport.capabilities, extent);
    // set chain to replace
    info().oldSwapchain = get();
    auto new_chain = (*m_device)->createSwapchainKHR(info(), nullptr);
    // destroy old chain
    replace(std::move(new_chain));

    auto m_images_swap = (*m_device)->getSwapchainImagesKHR(get());
    auto image_info = chain_to_img(info());

    m_views_swap.clear();
    m_views_swap = {m_images_swap.size(), Deleter<VkImageView>{*m_device, vkDestroyImageView}};
    for (uint32_t i = 0; i < m_images_swap.size(); i++) {
      m_views_swap[i] = createImageView(*m_device, m_images_swap[i], image_info);
    }
  }

  std::vector<Deleter<VkImageView>> const& views() const {
    return m_views_swap;
  }

  VkImageView const& view(std::size_t i) const {
    return m_views_swap[i].get();
  }

  vk::ImageCreateInfo imgInfo() const {
    return chain_to_img(info());
  }

  std::size_t numImages() const {
    return m_views_swap.size();
  }

  vk::Format format() const {
    return info().imageFormat;
  }

  vk::Extent2D const& extent() const {
    return info().imageExtent;
  }
 private:

  void destroy() override { 
    (*m_device)->destroySwapchainKHR(get());
  }

  // vk::Device m_device;
  Device const* m_device;
  vk::PhysicalDevice m_phys_device;
  std::vector<Deleter<VkImageView>> m_views_swap;
};

#endif