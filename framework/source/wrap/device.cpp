#include "wrap/device.hpp"

#include "wrap/device.hpp"

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <set>

uint32_t find_queue_family(vk::PhysicalDevice device, vk::QueueFlags const& flags) {
  auto queue_families = device.getQueueFamilyProperties();
  // find optimal queue
  for (size_t i = 0; i < queue_families.size(); ++i) {
    // flags must be identical
    if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags == flags) {
        return uint32_t(i);
    }
  }
  // find suitable queue
  for (size_t i = 0; i < queue_families.size(); ++i) {
    if (queue_families[i].queueCount > 0 && (queue_families[i].queueFlags & flags)) {
        return uint32_t(i);
    }
  }
  throw std::runtime_error{"no suiable queue found"};
  return 0;
}


Device::Device()
 :WrapperDevice{}
 ,m_queue_indices{}
 ,m_queues{}
 ,m_extensions{}
{}

Device::Device(vk::PhysicalDevice const& phys_dev, QueueFamilyIndices const& queues, std::vector<const char*> const& deviceExtensions)
 :Device{}
 {
  m_phys_device = phys_dev;
  m_extensions = deviceExtensions;
  m_queue_indices.emplace("graphics", queues.graphicsFamily);
  if (queues.presentFamily >= 0) {
    m_queue_indices.emplace("present", queues.presentFamily);
  }
  m_queue_indices.emplace("transfer", find_queue_family(physical(), vk::QueueFlagBits::eTransfer));
  // m_queue_indices.emplace("transfer", queues.graphicsFamily);

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<int> uniqueQueueFamilies{};
  for(auto const& index : m_queue_indices) {
    uniqueQueueFamilies.emplace(index.second);
  }
  // reserve enough space for all queues
  std::vector<float> queue_priorities(m_queue_indices.size(), 1.0f);
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
    queueCreateInfo.pQueuePriorities = queue_priorities.data();
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

  m_object = phys_dev.createDevice(info());

  std::map<int, uint> num_used{};
  for(auto const& index : m_queue_indices) {
    if(num_used.find(index.second) == num_used.end()) {
      num_used.emplace(index.second, 0);
    }
    m_queues.emplace(index.first, get().getQueue(index.second, num_used.at(index.second)));
    ++num_used.at(index.second);
  }
  // hack to keep compatibility with worker apps that wait on present queue
  if (queues.presentFamily < 0) {
    m_queues.emplace("present", m_queues.at("graphics"));
  }
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
  std::swap(m_extensions, dev.m_extensions);
}

vk::PhysicalDevice const& Device::physical() const {
  return m_phys_device;
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