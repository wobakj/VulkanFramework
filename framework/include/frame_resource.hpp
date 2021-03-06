#ifndef FRAME_RESOURCE_HPP
#define FRAME_RESOURCE_HPP

#include <vulkan/vulkan.hpp>

#include "wrap/fence.hpp"
#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/image_view.hpp"
#include "wrap/query_pool.hpp"
#include "wrap/descriptor_set.hpp"
#include "wrap/command_buffer.hpp"

#include <vector>

class FrameResource {
 public:
  FrameResource()
   :m_device{nullptr}
   ,num_uploads{0.0}
  {}

  FrameResource(Device& dev)
   :m_device(&dev)
   ,num_uploads{0.0}
   {}

  FrameResource(FrameResource&& rhs)
   :FrameResource{}
   {
    swap(rhs);
   }

  ~FrameResource() {
    for(auto const& semaphore : semaphores) {
      (*m_device)->destroySemaphore(semaphore.second);    
    }
  }

  FrameResource(FrameResource const&) = delete;
  FrameResource& operator=(FrameResource const&) = delete;

  FrameResource& operator=(FrameResource&& rhs) {
    swap(rhs);
    return *this;
  }

  void addSemaphore(std::string const& name) {
    semaphores.emplace(name, (*m_device)->createSemaphore({}));
  }

  void addFence(std::string const& name) {
   fences.emplace(name, Fence{(*m_device), vk::FenceCreateFlagBits::eSignaled});
  }

  void setCommandBuffer(std::string const& name, CommandBuffer&& buffer) {
    command_buffers[name] = std::move(buffer);
  }

  vk::Semaphore const& semaphore(std::string const& name) const {
    return semaphores.at(name);
  }

  Fence& fence(std::string const& name) {
    return fences.at(name);
  }

  CommandBuffer& commandBuffer(std::string const& name) {
    return command_buffers.at(name);
  }

  void waitFences() const {
    std::vector<vk::Fence> wait_fences;
    for(auto const& pair_fence : fences) {
      wait_fences.emplace_back(pair_fence.second);
    }
    if (!wait_fences.empty()) {
      // m_device->waitFences(wait_fences);
      if ((*m_device)->waitForFences(wait_fences, VK_TRUE, 100000000) != vk::Result::eSuccess) {
        throw std::runtime_error{"waited too long for fence"};
      }
    }
  }

  void swap(FrameResource& rhs) {
    std::swap(image, rhs.image);
    std::swap(target_region, rhs.target_region);
    std::swap(index, rhs.index);
    std::swap(m_device, rhs.m_device);
    std::swap(command_buffers, rhs.command_buffers);
    std::swap(semaphores, rhs.semaphores);
    std::swap(fences, rhs.fences);
    std::swap(descriptor_sets, rhs.descriptor_sets);
    std::swap(buffers, rhs.buffers);
    std::swap(buffer_views, rhs.buffer_views);
    std::swap(query_pools, rhs.query_pools);
    std::swap(num_uploads, rhs.num_uploads);
  }
  // for presenting
  uint32_t image; 
  // for drawing
  ImageLayers target_region;
  // for transferring between queues
  uint32_t index;
  std::map<std::string, CommandBuffer> command_buffers;
  std::map<std::string, vk::Semaphore> semaphores;
  std::map<std::string, Fence> fences;
  std::map<std::string, DescriptorSet> descriptor_sets;
  std::map<std::string, Buffer> buffers;
  std::map<std::string, BufferView> buffer_views;
  std::map<std::string, QueryPool> query_pools;

 private:
  Device const* m_device;
 public:
  double num_uploads;
};

#endif