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
 :WrapperDevice{}
 ,m_queue_indices{}
 ,m_queues{}
 ,m_pools{}
 ,m_extensions{}
 ,m_command_buffer_help{VK_NULL_HANDLE}
{}

Device::Device(vk::PhysicalDevice const& phys_dev, QueueFamilyIndices const& queues, std::vector<const char*> const& deviceExtensions)
 :Device{}
 {
  m_phys_device = phys_dev;
  m_extensions = deviceExtensions;
  m_queue_indices.emplace("graphics", queues.graphicsFamily);
  m_queue_indices.emplace("present", queues.presentFamily);
  m_queue_indices.emplace("transfer", queues.graphicsFamily);

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies{};
  for(auto const& index : m_queue_indices) {
    uniqueQueueFamilies.emplace(index.second);
  }

  float queuePriority = 1.0f;
  for (int queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }
  
  vk::PhysicalDeviceFeatures deviceFeatures{};
  info().pQueueCreateInfos = queueCreateInfos.data();
  info().queueCreateInfoCount = uint32_t(queueCreateInfos.size());
  info().pEnabledFeatures = &deviceFeatures;

  info().enabledExtensionCount = uint32_t(deviceExtensions.size());
  info().ppEnabledExtensionNames = deviceExtensions.data();

  get() = phys_dev.createDevice(info());

  for(auto const& index : m_queue_indices) {
    m_queues.emplace(index.first, get().getQueue(index.second, 0));   
  }

  createCommandPools();
}

void Device::createCommandPools() {
  vk::CommandPoolCreateInfo poolInfo{};
  poolInfo.queueFamilyIndex = getQueueIndex("graphics");
  poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

  m_pools.emplace("graphics", get().createCommandPool(poolInfo));
  // allocate pool for one-time commands, reset when beginning buffer
  poolInfo.queueFamilyIndex = getQueueIndex("transfer");
  poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

  m_pools.emplace("transfer", get().createCommandPool(poolInfo));
  // create buffer for onetime commands
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = pool("transfer");
  allocInfo.commandBufferCount = 1;

  m_command_buffer_help = get().allocateCommandBuffers(allocInfo)[0];
}

std::vector<uint32_t> Device::ownerIndices() const {
  std::set<uint32_t> uniqueQueueFamilies{};
  for(auto const& index : m_queue_indices) {
    uniqueQueueFamilies.emplace(index.second);
  }
  std::vector<uint32_t> owners{};
  for(auto const& index : uniqueQueueFamilies) {
    owners.emplace_back(index);
  }

  return owners;
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
  for(auto& pool : m_pools) {
    get().destroyCommandPool(pool.second);
  }
  get().freeCommandBuffers(pool("transfer"), {m_command_buffer_help});
  get().destroy();
}

 Device& Device::operator=(Device&& dev) {
  swap(dev);
  return *this;
 }

 void Device::swap(Device& dev) {
  WrapperDevice::swap(dev);
  std::swap(m_phys_device, dev.m_phys_device);
  std::swap(m_queues, dev.m_queues);
  std::swap(m_queue_indices, dev.m_queue_indices);
  std::swap(m_pools, dev.m_pools);
  std::swap(m_extensions, dev.m_extensions);
  std::swap(m_command_buffer_help, dev.m_command_buffer_help);
 }

 vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
 }

vk::CommandPool const& Device::pool(std::string const& name) const {
  return m_pools.at(name);
}
vk::Queue const& Device::getQueue(std::string const& name) const {
  return m_queues.at(name);
}
uint32_t Device::getQueueIndex(std::string const& name) const {
  return m_queue_indices.at(name);
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

  getQueue("transfer").submit({submitInfo}, VK_NULL_HANDLE);
  getQueue("transfer").waitIdle();
  m_command_buffer_help.reset({});
}
