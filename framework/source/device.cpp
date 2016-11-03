#include "device.hpp"

#include "buffer.hpp"
#include "image.hpp"
#include "pixel_data.hpp"
#include "swap_chain.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <set>

Device::Device()
 :Wrapper<vk::Device, vk::DeviceCreateInfo>{}
 ,m_queue_graphics{VK_NULL_HANDLE}
 ,m_queue_present{VK_NULL_HANDLE}
 ,m_index_graphics{-1}
 ,m_index_present{-1}
 ,m_extensions{}
 ,m_command_pool{VK_NULL_HANDLE}
 ,m_command_pool_help{VK_NULL_HANDLE}
 ,m_command_buffer_help{VK_NULL_HANDLE}
{}

Device::Device(vk::PhysicalDevice const& phys_dev, QueueFamilyIndices const& queues, std::vector<const char*> const& deviceExtensions)
 :Device{}
 {
  m_phys_device = phys_dev;
  m_index_graphics = queues.graphicsFamily;
  m_index_present = queues.presentFamily;
  m_extensions = deviceExtensions;

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies = {queues.graphicsFamily, queues.presentFamily};

  float queuePriority = 1.0f;
  for (int queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }
  
  vk::PhysicalDeviceFeatures deviceFeatures{};
  vk::DeviceCreateInfo createInfo{};
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  vk::DeviceCreateInfo a{createInfo};
  phys_dev.createDevice(&a, nullptr, &get());

  m_queue_graphics = get().getQueue(queues.graphicsFamily, 0);
  m_queue_present = get().getQueue(queues.presentFamily, 0);
  // throw std::exception();

  createCommandPools();
}

void Device::createCommandPools() {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.queueFamilyIndex = m_index_graphics;

  poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  m_command_pool = get().createCommandPool(poolInfo);
  // allocate pool for one-time commands, reset when beginning buffer
  poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  m_command_pool_help = get().createCommandPool(poolInfo);
  // create buffer for onetime commands
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = m_command_pool_help;
  allocInfo.commandBufferCount = 1;

  m_command_buffer_help = get().allocateCommandBuffers(allocInfo)[0];
}

SwapChain Device::createSwapChain(vk::SurfaceKHR const& surf, vk::Extent2D const& extend) const {
  SwapChain chain{};
  chain.create(*this, surf, extend);
  return chain; 
}

Device::Device(Device && dev)
 :Device{}
 {
  swap(dev);
 }

void Device::destroy() {
  get().destroyCommandPool(m_command_pool);
  get().destroyCommandPool(m_command_pool_help);
  get().freeCommandBuffers(pool(), {m_command_buffer_help});
  get().destroy();
}

 Device& Device::operator=(Device&& dev) {
  swap(dev);
  return *this;
 }

 void Device::swap(Device& dev) {
  // std::swap(m_device, dev.m_device);
  std::swap(get(), dev.get());
  std::swap(m_phys_device, dev.m_phys_device);
  std::swap(m_queue_graphics, dev.m_queue_graphics);
  std::swap(m_queue_present, dev.m_queue_present);
  std::swap(m_index_graphics, dev.m_index_graphics);
  std::swap(m_index_present, dev.m_index_present);
  std::swap(m_extensions, dev.m_extensions);
  std::swap(m_command_pool, dev.m_command_pool);
  std::swap(m_command_pool_help, dev.m_command_pool_help);
  std::swap(m_command_buffer_help, dev.m_command_buffer_help);
 }

 vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
 }

vk::CommandPool const& Device::pool() const {
  return m_command_pool;
}

vk::CommandPool const& Device::poolHelper() const {
  return m_command_pool_help;
}

vk::Queue const& Device::queueGraphics() const {
  return m_queue_graphics;
}

vk::Queue const& Device::queuePresent() const {
  return m_queue_present;
}

int const& Device::indexGraphics() const {
  return m_index_graphics;
}

int const& Device::indexPresent() const {
  return m_index_present;
}

Buffer Device::createBuffer(vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties) const {
  return Buffer{*this, size, usage, memProperties};
}
Buffer Device::createBuffer(void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) const {
  return Buffer{*this, data, size, usage};
}

Image Device::createImage(std::uint32_t width, std::uint32_t height, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage, vk::MemoryPropertyFlags const& mem_flags) const {
  return Image{*this, width, height, format, tiling, usage, mem_flags};
}

Image Device::createImage(pixel_data const& pixel_input, vk::ImageUsageFlags const& usage, vk::ImageLayout const& layout) const {
  return Image{*this, pixel_input, usage, layout};
}

void Device::copyBuffer(vk::Buffer const srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize const& size) const {
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();

  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, {copyRegion});

  endSingleTimeCommands();
}

void Device::copyImage(vk::Image const srcImage, vk::Image dstImage, uint32_t width, uint32_t height) const {
  vk::ImageSubresourceLayers subResource{};
  subResource.aspectMask = vk::ImageAspectFlagBits::eColor;
  subResource.baseArrayLayer = 0;
  subResource.mipLevel = 0;
  subResource.layerCount = 1;

  vk::ImageCopy region{};
  region.srcSubresource = subResource;
  region.dstSubresource = subResource;
  region.srcOffset = vk::Offset3D{0, 0, 0};
  region.dstOffset = vk::Offset3D{0, 0, 0};
  region.extent.width = width;
  region.extent.height = height;
  region.extent.depth = 1;

  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyImage(
    srcImage, vk::ImageLayout::eTransferSrcOptimal,
    dstImage, vk::ImageLayout::eTransferDstOptimal,
    1, &region
  );
  endSingleTimeCommands();
}



vk::CommandBuffer const& Device::beginSingleTimeCommands() const {
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  m_command_buffer_help.begin(beginInfo);

  return m_command_buffer_help;
}

void Device::endSingleTimeCommands() const {
  m_command_buffer_help.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_command_buffer_help;

  queueGraphics().submit({submitInfo}, VK_NULL_HANDLE);
  queueGraphics().waitIdle();
  m_command_buffer_help.reset({});
}
