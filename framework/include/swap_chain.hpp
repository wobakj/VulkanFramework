#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include "deleter.hpp"
#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

class Device;

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

QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass);
vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass, std::vector<vk::ImageView> attachments);

vk::ImageView createImageView(vk::Device const& device, vk::Image const& image, vk::ImageCreateInfo const& img_info);

using WrapperSwap = Wrapper<vk::SwapchainKHR, vk::SwapchainCreateInfoKHR>;
class SwapChain : public WrapperSwap {
 public:
  
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;

  SwapChain();
  SwapChain(SwapChain && chain);

  SwapChain& operator=(SwapChain&& chain);

  void swap(SwapChain& chain);

  void create(Device const& dev, vk::SurfaceKHR const& surface, VkExtent2D const& extent);
  void recreate(vk::Extent2D const& extent);

  VkImageView const& view(std::size_t i) const;

  std::vector<Deleter<VkImageView>> const& views() const;
  vk::ImageCreateInfo imgInfo() const;
  std::size_t numImages() const;
  vk::Format format() const;
  vk::Extent2D const& extent() const;

 private:

  void destroy() override ;

  Device const* m_device;
  std::vector<Deleter<VkImageView>> m_views_swap;
};

#endif