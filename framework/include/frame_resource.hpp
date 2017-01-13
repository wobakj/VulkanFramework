#ifndef FRAME_RESOURCE_HPP
#define FRAME_RESOURCE_HPP

#include <vulkan/vulkan.hpp>

#include "fence.hpp"
#include "buffer.hpp"
#include "buffer_view.hpp"
#include "query_pool.hpp"

#include <vector>

class FrameResource {
 public:
  FrameResource()
   :m_device{nullptr}
  {}

  FrameResource(Device& dev)
   :m_device(&dev)
   {
    // frame resource always has these objects
    semaphores.emplace("acquire", (*m_device)->createSemaphore({}));
    semaphores.emplace("draw", (*m_device)->createSemaphore({}));
    fences.emplace("draw", Fence{dev, vk::FenceCreateFlagBits::eSignaled});
    fences.emplace("acquire", Fence{dev, vk::FenceCreateFlagBits::eSignaled});
    command_buffers.emplace("draw", m_device->createCommandBuffer("graphics"));
  }

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

  vk::Semaphore const& semaphoreAcquire() {
    return semaphores.at("acquire");
  }

  vk::Semaphore const& semaphoreDraw() {
    return semaphores.at("draw");
  }

  Fence& fenceDraw() {
    return fences.at("draw");
  }

  Fence& fenceAcquire() {
    return fences.at("acquire");
  }

  void waitFences() const {
    std::vector<vk::Fence> wait_fences;
    for(auto const& pair_fence : fences) {
      wait_fences.emplace_back(pair_fence.second);
    }
    m_device->waitFences(wait_fences);
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
  double num_uploads;

 private:
  Device const* m_device;
};

#endif