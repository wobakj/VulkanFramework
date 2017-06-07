#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include "deleter.hpp"
#include "wrap/wrapper.hpp"
#include "wrap/image_view.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>

class Device;
class SwapChain;
struct QueueFamilyIndices;

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

vk::ImageCreateInfo chain_to_img(SwapChain const& swap_info);

vk::ImageView createImageView(vk::Device const& device, vk::Image const& image, vk::ImageCreateInfo const& img_info);

using WrapperSwap = Wrapper<vk::SwapchainKHR, vk::SwapchainCreateInfoKHR>;
class SwapChain : public WrapperSwap {
 public:
  
  SwapChain(SwapChain const&) = delete;
  SwapChain& operator=(SwapChain const&) = delete;
  ~SwapChain();

  SwapChain();
  SwapChain(SwapChain && chain);

  SwapChain& operator=(SwapChain&& chain);

  void swap(SwapChain& chain);

  void create(Device const& dev, vk::SurfaceKHR const& surface, VkExtent2D const& extent, vk::PresentModeKHR const& present_mode, uint32_t num_images);
  void recreate(vk::Extent2D const& extent);

  void layoutTransitionCommand(vk::CommandBuffer const& buffer, uint32_t index, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const;

  std::vector<ImageView> const& views() const;
  ImageView const& view(std::size_t i) const;
  
  std::vector<vk::Image> const& images() const;
  vk::Image const& image(uint32_t index) const;

  vk::ImageCreateInfo imgInfo() const;
  uint32_t numImages() const;
  vk::Format format() const;
  vk::Extent2D const& extent() const;
  float aspect() const;
  vk::Viewport asViewport() const;
  vk::Rect2D asRect() const;
  vk::ImageLayout const& layout() const;

 private:

  void destroy() override ;

  Device const* m_device;
  std::vector<vk::Image> m_images_swap;
  std::vector<ImageView> m_views_swap;
  vk::ImageLayout m_layout;
};

#endif