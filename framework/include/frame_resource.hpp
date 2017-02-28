#ifndef FRAME_RESOURCE_HPP
#define FRAME_RESOURCE_HPP

#include <vulkan/vulkan.hpp>

#include "wrap/fence.hpp"
#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/query_pool.hpp"

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
    // free resources
    for(auto const& command_buffer : command_buffers) {
      (*m_device)->freeCommandBuffers(m_device->pool("graphics"), {command_buffer.second});    
    }
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

  void addCommandBuffer(std::string const& name, vk::CommandBuffer && buffer) {
    command_buffers.emplace(name, buffer);
  }

  vk::Semaphore const& semaphore(std::string const& name) {
    return semaphores.at(name);
  }

  Fence& fence(std::string const& name) {
    return fences.at(name);
  }

  vk::CommandBuffer& commandBuffer(std::string const& name) {
    return command_buffers.at(name);
  }

  void waitFences() const {
    std::vector<vk::Fence> wait_fences;
    for(auto const& pair_fence : fences) {
      wait_fences.emplace_back(pair_fence.second);
    }
    if (!wait_fences.empty()) {
      m_device->waitFences(wait_fences);
    }
  }

  void swap(FrameResource& rhs) {
    std::swap(image, rhs.image);
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

  uint32_t image;
  std::map<std::string, vk::CommandBuffer> command_buffers;
  std::map<std::string, vk::Semaphore> semaphores;
  std::map<std::string, Fence> fences;
  std::map<std::string, vk::DescriptorSet> descriptor_sets;
  std::map<std::string, Buffer> buffers;
  std::map<std::string, BufferView> buffer_views;
  std::map<std::string, QueryPool> query_pools;

 private:
  Device const* m_device;
 public:
  double num_uploads;
};

#endif