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
  m_info.pQueueCreateInfos = queueCreateInfos.data();
  m_info.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
  m_info.pEnabledFeatures = &deviceFeatures;

  m_info.enabledExtensionCount = uint32_t(deviceExtensions.size());
  m_info.ppEnabledExtensionNames = deviceExtensions.data();

  m_object =phys_dev.createDevice(info());

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

Device::~Device() {
  cleanup();
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
  // std::swap(m_mutex_single_command, dev.m_mutex_single_command);
 }

 vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
 }

vk::CommandPool const& Device::pool(std::string const& name) const {
  return m_pools.at(name);
}

std::vector<vk::CommandBuffer> Device::createCommandBuffers(std::string const& name_pool, vk::CommandBufferLevel const& level, uint32_t num) const {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.setCommandPool(pool(name_pool));
  allocInfo.setLevel(level);
  allocInfo.setCommandBufferCount(num);
  return get().allocateCommandBuffers(allocInfo);
}

vk::CommandBuffer Device::createCommandBuffer(std::string const& name_pool, vk::CommandBufferLevel const& level) const {
  return createCommandBuffers(name_pool, level, 1)[0];
}

vk::Queue const& Device::getQueue(std::string const& name) const {
  return m_queues.at(name);
}
uint32_t Device::getQueueIndex(std::string const& name) const {
  return m_queue_indices.at(name);
}

Buffer Device::createBuffer(vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) const {
  return Buffer{*this, size, usage};
}

void Device::adjustStagingPool(vk::DeviceSize const& size) {
  if (m_pools_memory.find("stage") == m_pools_memory.end() || memoryPool("stage").size() < size) {
    // create new staging buffer
    m_buffer_stage = std::unique_ptr<Buffer>{new Buffer{*this, size, vk::BufferUsageFlagBits::eTransferSrc}};
    reallocateMemoryPool("stage", m_buffer_stage->memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, size);
    m_buffer_stage->bindTo(memoryPool("stage"), 0);
  }
}

Image Device::createImage(vk::Extent3D const& extent, vk::Format const& format, vk::ImageTiling const& tiling, vk::ImageUsageFlags const& usage) const {
  return Image{*this, extent, format, tiling, usage};
}

void Device::uploadImageData(void const* data_ptr, Image& image) {
  Image image_stage{*this, image.info().extent, image.format(), vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc};
  auto prev_layout = image.layout();
  { //lock staging memory
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(image.size());
    image_stage.bindTo(memoryPool("stage"), 0);
    // data size is dependent on target image size, not input data size!
    image_stage.setData(data_ptr, image.size());
    image_stage.transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);

    image.transitionToLayout(vk::ImageLayout::eTransferDstOptimal);
    copyImage(image_stage, image, image.info().extent.width, image.info().extent.height);
  }

  image.transitionToLayout(prev_layout);
}

void Device::uploadBufferData(void const* data_ptr, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  uploadBufferData(data_ptr, buffer.size(), buffer, dst_offset);
}

void Device::uploadBufferData(void const* data_ptr, vk::DeviceSize const& size, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(size);
    m_buffer_stage->setData(data_ptr, size, 0);

    copyBuffer(*m_buffer_stage, buffer, size, 0, dst_offset);
  }
}

void Device::copyBuffer(vk::Buffer const srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize const& size, vk::DeviceSize const& src_offset, vk::DeviceSize const& dst_offset) const {
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();

  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  copyRegion.srcOffset = src_offset;
  copyRegion.dstOffset = dst_offset;
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

void Device::transitionToLayout(vk::Image const& img, vk::ImageCreateInfo const& info, vk::ImageLayout const& newLayout) const {
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
// todo: use image memory barrier instead of pipeline
  commandBuffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::DependencyFlags{},
    {},
    {},
    {barrier}
  );
  endSingleTimeCommands();
  // store new layout
  // m_info.initialLayout = newLayout;
}

vk::CommandBuffer const& Device::beginSingleTimeCommands() const {
  m_mutex_single_command.lock();
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  m_command_buffer_help.begin(beginInfo);

  return m_command_buffer_help;
}

void Device::waitFence(vk::Fence const& fence) const {
  if (fence) {
    // only try to wait if fence is actually in use
    if (get().getFenceStatus(fence) == vk::Result::eNotReady) {
      if (get().waitForFences({fence}, VK_TRUE, 100000000) != vk::Result::eSuccess) {
        throw std::exception();
      }
      get().resetFences({fence});
    }
  }
}

void Device::endSingleTimeCommands() const {
  m_command_buffer_help.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_command_buffer_help;

  getQueue("transfer").submit({submitInfo}, VK_NULL_HANDLE);
  getQueue("transfer").waitIdle();
  m_command_buffer_help.reset({});

  m_mutex_single_command.unlock();
}

Memory& Device::memoryPool(std::string const& name) {
  return m_pools_memory.at(name);
}

void Device::allocateMemoryPool(std::string const& name, uint32_t type_bits, vk::MemoryPropertyFlags const& mem_flags, vk::DeviceSize const& size) {
  // std::cout << "allocating pool " << name << " type " << type << ", size " << size << std::endl;
  m_pools_memory.emplace(name, Memory{*this, findMemoryType(physical(), type_bits, mem_flags), size});
}

void Device::reallocateMemoryPool(std::string const& name, uint32_t type_bits, vk::MemoryPropertyFlags const& mem_flags, vk::DeviceSize const& size) {
  // std::cout << "allocating pool " << name << " type " << type << ", size " << size << std::endl;
  m_pools_memory[name] = Memory{*this, findMemoryType(physical(), type_bits, mem_flags), size};
}

