#include "device.hpp"

#include "buffer.hpp"
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
{}

void Device::create(vk::PhysicalDevice const& phys_dev, int graphics, int present, std::vector<const char*> const& deviceExtensions) {
  m_phys_device = phys_dev;
  m_index_graphics = graphics;
  m_index_present = present;
  m_extensions = deviceExtensions;

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies = {graphics, present};

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

  m_queue_graphics = get().getQueue(graphics, 0);
  m_queue_present = get().getQueue(present, 0);
  // throw std::exception();

  createCommandPool();
}

void Device::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.queueFamilyIndex = m_index_graphics;

  m_command_pool = get().createCommandPool(poolInfo);
}

SwapChain Device::createSwapChain(vk::SurfaceKHR const& surf, vk::Extent2D const& extend) const {
  SwapChain chain{};
  chain.create(*this, m_phys_device, surf, extend);
  return chain; 
}

Device::Device(Device && dev)
 :Device{}
 {
  swap(dev);
 }

void Device::destroy() {
  get().destroyCommandPool(m_command_pool);
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
 }

 vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
 }

vk::CommandPool const& Device::pool() const {
  return m_command_pool;
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

void Device::copyBuffer(vk::Buffer const srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize const& size) const {
  vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, {copyRegion});

  endSingleTimeCommands(commandBuffer);
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

  vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
  commandBuffer.copyImage(
    srcImage, vk::ImageLayout::eTransferSrcOptimal,
    dstImage, vk::ImageLayout::eTransferDstOptimal,
    1, &region
  );
  endSingleTimeCommands(commandBuffer);
}


Buffer Device::createBuffer(void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) const {
  return Buffer{*this, data, size, usage};
}

vk::CommandBuffer Device::beginSingleTimeCommands() const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = pool();
  allocInfo.commandBufferCount = 1;

  vk::CommandBuffer commandBuffer = get().allocateCommandBuffers(allocInfo)[0];

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void Device::endSingleTimeCommands(vk::CommandBuffer& commandBuffer) const {
  commandBuffer.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  queueGraphics().submit({submitInfo}, VK_NULL_HANDLE);
  queueGraphics().waitIdle();

  get().freeCommandBuffers(pool(), {commandBuffer});
  commandBuffer = VK_NULL_HANDLE;
}
