#include "wrap/device.hpp"

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/image.hpp"
#include "wrap/pixel_data.hpp"
#include "wrap/swap_chain.hpp"

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
    std::cout << "creating " << num << " queues from family " << queueFamily << std::endl;
  }
  
  vk::PhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.fillModeNonSolid = true;
  deviceFeatures.wideLines = true;
  deviceFeatures.independentBlend = true;
  deviceFeatures.multiDrawIndirect = true;
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

  poolInfo.queueFamilyIndex = getQueueIndex("graphics");
  poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

  m_pools.emplace("compute", get().createCommandPool(poolInfo));

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

uint32_t Device::suitableMemoryTypes(vk::BufferUsageFlags const& usage) const {
  vk::BufferCreateInfo info;
  info.size = 1;
  info.usage = usage;
  auto buffer = get().createBuffer(info);
  auto bits = get().getBufferMemoryRequirements(buffer).memoryTypeBits;
  get().destroyBuffer(buffer);
  return bits;
}

uint32_t Device::suitableMemoryTypes(vk::Format const& format, vk::ImageTiling const& tiling) const {
  vk::ImageCreateInfo info;
  info.extent = vk::Extent3D{1, 1, 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.usage = vk::ImageUsageFlagBits::eTransferSrc;
  info.format = format;
  info.tiling = tiling;
  auto image = get().createImage(info);
  auto bits = get().getImageMemoryRequirements(image).memoryTypeBits;
  get().destroyImage(image);
  return bits;
}

uint32_t Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) const {
  auto memProperties = physical().getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (index_matches_filter(i, typeFilter) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
  return 0;
}

uint32_t Device::suitableMemoryType(vk::BufferUsageFlags const& usage, vk::MemoryPropertyFlags const& properties) const {
  return findMemoryType(suitableMemoryTypes(usage), properties);
}

uint32_t Device::suitableMemoryType(vk::Format const& format, vk::ImageTiling const& tiling, vk::MemoryPropertyFlags const& properties) const {
  return findMemoryType(suitableMemoryTypes(format, tiling), properties);
}

void Device::adjustStagingPool(vk::DeviceSize const& size) {
  if (m_pools_memory.find("stage") == m_pools_memory.end() || memoryPool("stage").size() < size) {
    // create new staging buffer
    m_buffer_stage = std::unique_ptr<Buffer>{new Buffer{*this, size, vk::BufferUsageFlagBits::eTransferSrc}};
    reallocateMemoryPool("stage", m_buffer_stage->memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_buffer_stage->size());
    m_buffer_stage->bindTo(memoryPool("stage"), 0);
  }
}


void Device::uploadBufferData(void const* data_ptr, BufferView& buffer_view) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(buffer_view.size());
    m_buffer_stage->setData(data_ptr, buffer_view.size(), 0);

    copyBuffer(m_buffer_stage->get(), buffer_view.buffer(), buffer_view.size(), 0, buffer_view.offset());
  }
}

void Device::uploadBufferData(void const* data_ptr, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  uploadBufferData(data_ptr, buffer.size(), buffer, dst_offset);
}

void Device::uploadBufferData(void const* data_ptr, vk::DeviceSize const& size, Buffer& buffer, vk::DeviceSize const& dst_offset) {
  { //lock staging memory and buffer
    std::lock_guard<std::mutex> lock{m_mutex_staging};
    adjustStagingPool(size);
    m_buffer_stage->setData(data_ptr, size, 0);

    copyBuffer(m_buffer_stage->get(), buffer.get(), size, 0, dst_offset);
  }
}

void Device::copyBuffer(vk::Buffer const& srcBuffer, vk::Buffer const& dstBuffer, vk::DeviceSize const& size, vk::DeviceSize const& src_offset, vk::DeviceSize const& dst_offset) const {
  vk::CommandBuffer const& commandBuffer = beginSingleTimeCommands();

  vk::BufferCopy copyRegion{};
  copyRegion.size = size;
  copyRegion.srcOffset = src_offset;
  copyRegion.dstOffset = dst_offset;
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, {copyRegion});

  endSingleTimeCommands();
}

vk::CommandBuffer const& Device::beginSingleTimeCommands() const {
  m_mutex_single_command.lock();
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

  m_mutex_single_command.unlock();
}

Memory& Device::memoryPool(std::string const& name) {
  return m_pools_memory.at(name);
}

void Device::allocateMemoryPool(std::string const& name, uint32_t type_bits, vk::MemoryPropertyFlags const& mem_flags, vk::DeviceSize const& size) {
  // std::cout << "allocating pool " << name << " type " << type << ", size " << size << std::endl;
  m_pools_memory.emplace(name, Memory{*this, findMemoryType(type_bits, mem_flags), size});
}

void Device::reallocateMemoryPool(std::string const& name, uint32_t type_bits, vk::MemoryPropertyFlags const& mem_flags, vk::DeviceSize const& size) {
  // std::cout << "allocating pool " << name << " type " << type << ", size " << size << std::endl;
  m_pools_memory[name] = Memory{*this, findMemoryType(type_bits, mem_flags), size};
}