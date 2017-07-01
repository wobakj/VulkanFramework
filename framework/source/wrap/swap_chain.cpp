#include "wrap/swap_chain.hpp"

#include "deleter.hpp"
#include "wrap/wrapper.hpp"
#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "wrap/image_view.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
  QueueFamilyIndices indices;

  auto queueFamilies = device.getQueueFamilyProperties();
  
  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphicsFamily = i;
        if (!surface) {
          break;
        }
    }
    
    if (surface) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
      if (queueFamily.queueCount > 0 && presentSupport) {
        indices.presentFamily = i;
      }

      if (indices.isComplete()) {
        break;
      }
    }

    i++;
  }
  return indices;
}

  SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
  }

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      VkExtent2D actualExtent = extent;

      actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
      actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
      
      return actualExtent;
    }
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
      return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
    }

    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
          return availableFormat;
      }
    }

    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes, vk::PresentModeKHR const& preferred) {
      for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == preferred) {
          return availablePresentMode;
        }
      }
      std::cout << "Present mode " << to_string(preferred)  << " not avaible, falling back to FIFO" << std::endl;
      return vk::PresentModeKHR::eFifo;
  }

static vk::ImageCreateInfo chain_to_img(vk::SwapchainCreateInfoKHR const& info) {
  vk::ImageCreateInfo img_info{};
  img_info.imageType = vk::ImageType::e2D;
  img_info.tiling = vk::ImageTiling::eOptimal;
  img_info.mipLevels = 1;
  img_info.format = info.imageFormat;
  img_info.arrayLayers = info.imageArrayLayers;
  img_info.extent = vk::Extent3D{info.imageExtent.width, info.imageExtent.height, 0};
  img_info.usage = info.imageUsage;
  img_info.sharingMode = info.imageSharingMode;
  img_info.queueFamilyIndexCount = info.queueFamilyIndexCount;
  img_info.pQueueFamilyIndices = info.pQueueFamilyIndices;
  img_info.initialLayout = vk::ImageLayout::eUndefined;
  return img_info;
}

SwapChain::SwapChain()
 :WrapperSwap{}
 ,m_device{nullptr}
 ,m_images_swap{}
 ,m_views_swap{}
{}

SwapChain::SwapChain(SwapChain && chain)
 :SwapChain{}
 {
  swap(chain);
 }

SwapChain::~SwapChain() {
  cleanup();
}

 SwapChain& SwapChain::operator=(SwapChain&& chain) {
  swap(chain);
  return *this;
 }

 void SwapChain::swap(SwapChain& chain) {
  WrapperSwap::swap(chain);
  std::swap(m_device, chain.m_device);
  std::swap(m_views_swap, chain.m_views_swap);
  std::swap(m_images_swap, chain.m_images_swap);
 }

void SwapChain::create(Device const& device, vk::SurfaceKHR const& surface, VkExtent2D const& extent, vk::PresentModeKHR const& present_mode, uint32_t num_images) {
  m_device = &device;
  m_info.surface = surface;

  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.physical(), surface);

  m_info.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, present_mode);
  vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);

  if (swapChainSupport.capabilities.maxImageCount > 0 && num_images > swapChainSupport.capabilities.maxImageCount) {
    num_images = swapChainSupport.capabilities.maxImageCount;
  }
  std::cout << "swapchain has " << num_images << " images, format "<< to_string(surfaceFormat.format) << ", preset mode is " << to_string(m_info.presentMode) << std::endl;
  m_info.minImageCount = num_images;
  m_info.imageFormat = surfaceFormat.format;
  m_info.imageColorSpace = surfaceFormat.colorSpace;
  m_info.imageArrayLayers = 1;
  // m_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  m_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

  QueueFamilyIndices indices = findQueueFamilies(device.physical(), m_info.surface);
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

  recreate(extent);

}

void SwapChain::recreate(vk::Extent2D const& extent) {
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_device->physical(), m_info.surface);
  m_info.imageExtent = chooseSwapExtent(swapChainSupport.capabilities, extent);
  // set chain to replace
  m_info.oldSwapchain = get();
  auto new_chain = (*m_device)->createSwapchainKHR(info(), nullptr);

  replace(std::move(new_chain), m_info);

  m_images_swap = (*m_device)->getSwapchainImagesKHR(get());
  // transition images to present in case no render pass is applied

  auto image_info = chain_to_img(info());

  m_views_swap.clear();
  for (uint32_t i = 0; i < m_images_swap.size(); i++) {
    m_views_swap.emplace_back(ImageView{*m_device, m_images_swap[i], image_info});
  }
}

std::vector<ImageView> const& SwapChain::views() const {
  return m_views_swap;
}

std::vector<vk::Image> const& SwapChain::images() const {
  return  m_images_swap;
}

vk::Image const& SwapChain::image(uint32_t index) const {
  return  m_images_swap.at(index);
}

ImageView const& SwapChain::view(std::size_t i) const {
  return m_views_swap[i];
}

uint32_t SwapChain::numImages() const {
  return uint32_t(m_views_swap.size());
}

vk::Format SwapChain::format() const {
  return m_info.imageFormat;
}

vk::Extent2D const& SwapChain::extent() const {
  return m_info.imageExtent;
}

float SwapChain::aspect() const {
  return float(m_info.imageExtent.width) / float(m_info.imageExtent.height);
}

vk::Viewport SwapChain::asViewport() const {
  return vk::Viewport{0.0f, 0.0f, float(extent().width), float(extent().height), 0.0f, 1.0f};
}

vk::Rect2D SwapChain::asRect() const {
  return vk::Rect2D{vk::Offset2D{}, extent()};
}

void SwapChain::destroy() { 
  (*m_device)->destroySwapchainKHR(get());
}