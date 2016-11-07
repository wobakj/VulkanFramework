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
    uint num = 0;
    for(auto const& index : m_queue_indices) {
      if (queueFamily == int(index.second)) {
        ++num;
      }      
    }
    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = num;
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

  std::map<int, uint> num_used{};
  for(auto const& index : m_queue_indices) {
    if(num_used.find(index.second) == num_used.end()) {
      num_used.emplace(index.second, 0);
    }
    m_queues.emplace(index.first, get().getQueue(index.second, num_used.at(index.second)));   
    ++num_used.at(index.second);
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
  // std::swap(m_mutex, dev.m_mutex);
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

void Device::copyImage(Image const& srcImage, Image& dstImage, uint32_t width, uint32_t height) const {
  vk::ImageSubresourceLayers subResource{};
  if (is_depth(srcImage.format())) {
    subResource.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(srcImage.format())) {
      subResource.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    subResource.aspectMask = vk::ImageAspectFlagBits::eColor;
  }
  subResource.baseArrayLayer = 0;
  subResource.mipLevel = 0;
  subResource.layerCount = srcImage.info().arrayLayers;


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

void Device::transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& newLayout) {
  // get current layout form creation info
  vk::ImageLayout const& oldLayout = info.initialLayout;
  
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();
  vk::ImageMemoryBarrier barrier{};
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = img;

  if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    if (has_stencil(info.format)) {
      barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
  } 
  else {
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  }

  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = info.mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = info.arrayLayers;

  barrier.srcAccessMask = layout_to_access(oldLayout);
  barrier.dstAccessMask = layout_to_access(newLayout);

  commandBuffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
  endSingleTimeCommands();
  // store new layout
  // info().initialLayout = newLayout;
}

vk::CommandBuffer const& Device::beginSingleTimeCommands() const {
  m_mutex.lock();
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

  m_mutex.unlock();
}
