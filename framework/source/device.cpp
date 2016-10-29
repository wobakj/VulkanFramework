#include "device.hpp"

#include <vulkan/vulkan.hpp>
#include <iostream>
#include "device.hpp"
#include "swap_chain.hpp"
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


uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
  auto memProperties = device.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
  return 0;
}

std::pair<vk::Buffer, vk::DeviceMemory> Device::createBuffer(vk::DeviceSize const& size, vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& memProperties) {

  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  vk::Buffer buffer = get().createBuffer(bufferInfo);

  auto memRequirements = get().getBufferMemoryRequirements(buffer);

  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(physical(), memRequirements.memoryTypeBits, memProperties);
  vk::DeviceMemory memory = get().allocateMemory(allocInfo);

  get().bindBufferMemory(buffer, memory, 0);
  return std::make_pair(buffer, memory);
}

void Device::copyBuffer(VkBuffer const& srcBuffer, VkBuffer const& dstBuffer, VkDeviceSize const& size) {
  vk::CommandBufferAllocateInfo allocInfo{};
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = pool();
  allocInfo.commandBufferCount = 1;

  vk::CommandBuffer commandBuffer = get().allocateCommandBuffers(allocInfo)[0];
  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  commandBuffer.begin(beginInfo);
  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, {copyRegion});
  commandBuffer.end();

  vk::SubmitInfo submitInfo{};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  m_queue_graphics.submit(1, &submitInfo, VK_NULL_HANDLE);
  m_queue_graphics.waitIdle();
  get().freeCommandBuffers(pool(), {commandBuffer});
}

std::pair<vk::Buffer, vk::DeviceMemory> Device::createBuffer(void* data, vk::DeviceSize const& size, vk::BufferUsageFlags const& usage) {
  Deleter<VkBuffer> buffer_stage{*this, vkDestroyBuffer};
  Deleter<VkDeviceMemory> memory_stage{*this, vkFreeMemory};
  auto buff_mem_stage = createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  buffer_stage = std::move(buff_mem_stage.first);
  memory_stage = std::move(buff_mem_stage.second);
  
  void* buff_ptr = get().mapMemory(memory_stage.get(), 0, size);
  std::memcpy(buff_ptr, data, (size_t) size);
  get().unmapMemory(memory_stage.get());

  auto buff_mem = createBuffer(size, usage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
  // m_vertexBuffer = std::move(buff_mem.first);
  // m_vertexBufferMemory = std::move(buff_mem.second);

  copyBuffer(buffer_stage.get(), buff_mem.first, size);

  return buff_mem;
}