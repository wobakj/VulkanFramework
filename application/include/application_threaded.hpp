#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"
#include "double_buffer.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

struct frame_resources_t {
  frame_resources_t(Device& dev)
   :m_device(dev)
   ,first_acquire{true}
   ,first_draw{true}
   {
    semaphores.emplace("acquire", m_device->createSemaphore({}));
    semaphores.emplace("draw", m_device->createSemaphore({}));
    fences.emplace("draw", m_device->createFence({}));
    fences.emplace("acquire", m_device->createFence({}));
    // device->resetFences({fenceDraw()});
  }

  frame_resources_t(frame_resources_t&& res)
   :m_device(res.m_device)
   {
    std::swap(image, res.image);
    std::swap(command_buffers, res.command_buffers);
    std::swap(semaphores, res.semaphores);
    std::swap(fences, res.fences);
    std::swap(descriptor_sets, res.descriptor_sets);
   }

  frame_resources_t(frame_resources_t const&) = delete;
  frame_resources_t& operator=(frame_resources_t const&) = delete;

  Device& m_device;
  uint32_t image;
  std::map<std::string, vk::CommandBuffer> command_buffers;
  std::map<std::string, vk::Semaphore> semaphores;
  std::map<std::string, vk::Fence> fences;
  std::map<std::string, vk::DescriptorSet> descriptor_sets;
  // need to be atomic
  std::atomic<bool> first_acquire;
  std::atomic<bool> first_draw;

  vk::Semaphore const& semaphoreAcquire() {
    return semaphores.at("acquire");
  }

  vk::Semaphore const& semaphoreDraw() {
    return semaphores.at("draw");
  }

  vk::Fence const& fenceDraw() {
    return fences.at("draw");
  }

  vk::Fence const& fenceAcquire() {
    return fences.at("acquire");
  }

  void waitFences() const {
    std::vector<vk::Fence> wait_fences;
    for(auto const& pair_fence : fences) {
      wait_fences.emplace_back(pair_fence.second);
    }
    m_device.waitFences(wait_fences);
  }

  ~frame_resources_t() {
    // free resources
    for(auto const& command_buffer : command_buffers) {
      m_device->freeCommandBuffers(m_device.pool("graphics"), {command_buffer.second});    
    }
    for(auto const& semaphore : semaphores) {
      m_device->destroySemaphore(semaphore.second);    
    }
    for(auto const& fence : fences) {
      m_device->destroyFence(fence.second);    
    }
  }
};

class ApplicationThreaded : public Application {
 public:
  ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  ~ApplicationThreaded();

 private:
  void render() override;
  void createPrimaryCommandBuffer(frame_resources_t& res);
  
  void createLights();
  void updateLights();
  void loadModel();
  void createUniformBuffers();
  void createCommandBuffers(frame_resources_t& res);
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void resize() override;
  void recreatePipeline() override;
  void updateView() override;
  void updateCommandBuffers(frame_resources_t& res);
  void createFramebuffers();
  void createRenderPass();
  void createMemoryPools();
  void createGraphicsPipeline();
  void createDescriptorPool();
  void createDepthResource();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void drawLoop();
  void draw();
  void createFrameResources();
  void emptyDrawQueue();

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Model m_model;
  Model m_model_2;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;

  bool m_sphere;
  bool m_initializing;

  std::thread m_thread_load;
  std::thread m_thread_render;
  std::atomic<bool> m_model_dirty;
  std::atomic<bool> m_should_draw;
  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_record_queue;

  std::vector<frame_resources_t> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  void acquireImage(frame_resources_t& res);
  void present(frame_resources_t& res);
  void submitDraw(frame_resources_t& res);
};

#endif