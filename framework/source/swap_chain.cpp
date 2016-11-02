#include "swap_chain.hpp"

#include "deleter.hpp"
#include "wrapper.hpp"
#include "device.hpp"
#include "image.hpp"

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

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
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

  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes) {
      for (const auto& availablePresentMode : availablePresentModes) {
          if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
              return availablePresentMode;
          }
      }

      return vk::PresentModeKHR::eFifo;
  }

vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass) {
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

vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass, std::vector<vk::ImageView> attachments) {
  // vk::ImageView attachments[] = {view};
  attachments.insert(attachments.begin(),view);
  vk::FramebufferCreateInfo fb_info{};
  fb_info.renderPass = pass;
  fb_info.attachmentCount = std::uint32_t(attachments.size());
  fb_info.pAttachments = attachments.data();
  fb_info.width = img_info.extent.width;
  fb_info.height = img_info.extent.height;
  fb_info.layers = img_info.arrayLayers;
  return fb_info;
}

vk::ImageCreateInfo chain_to_img(vk::SwapchainCreateInfoKHR const& swap_info) {
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

vk::ImageView createImageView(vk::Device const& device, vk::Image const& image, vk::ImageCreateInfo const& img_info) {
  return device.createImageView(img_to_view(image, img_info), nullptr);
}

SwapChain::SwapChain()
 :WrapperSwap{}
 ,m_device{nullptr}
 // ,m_images_swap{}
 ,m_views_swap{}
{}

SwapChain::SwapChain(SwapChain && chain)
 :SwapChain{}
 {
  swap(chain);
 }

 SwapChain& SwapChain::operator=(SwapChain&& chain) {
  swap(chain);
  return *this;
 }

 void SwapChain::swap(SwapChain& chain) {
  WrapperSwap::swap(chain);
  std::swap(m_device, chain.m_device);
  std::swap(m_views_swap, chain.m_views_swap);
 }

void SwapChain::create(Device const& device, vk::SurfaceKHR const& surface, VkExtent2D const& extent) {
  m_device = &device;
  info().surface = surface;

  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.physical(), surface);

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

  QueueFamilyIndices indices = findQueueFamilies(device.physical(), info().surface);
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

void SwapChain::recreate(vk::Extent2D const& extent) {
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_device->physical(), info().surface);
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

std::vector<Deleter<VkImageView>> const& SwapChain::views() const {
  return m_views_swap;
}

VkImageView const& SwapChain::view(std::size_t i) const {
  return m_views_swap[i].get();
}

vk::ImageCreateInfo SwapChain::imgInfo() const {
  return chain_to_img(info());
}

std::size_t SwapChain::numImages() const {
  return m_views_swap.size();
}

vk::Format SwapChain::format() const {
  return info().imageFormat;
}

vk::Extent2D const& SwapChain::extent() const {
  return info().imageExtent;
}

void SwapChain::destroy() { 
  (*m_device)->destroySwapchainKHR(get());
}