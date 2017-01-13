#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "semaphore.hpp"
#include "frame_resource.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationThreaded : public Application {
 public:
  ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, std::vector<std::string> const& args, uint32_t num_frames);
  void emptyDrawQueue() override;

 protected:
  void createFrameResources();
  virtual FrameResource createFrameResource() = 0;
  void shutDown();
  void resize() override;
  void recreatePipeline() override;
  void startRenderThread();
  
  virtual void createCommandBuffers(FrameResource& res) = 0;
  virtual void updateCommandBuffers(FrameResource& res) = 0;
  virtual void updateDescriptors(FrameResource& resource) {};

  virtual void createFramebuffers() = 0;
  virtual void createFramebufferAttachments() = 0;
  virtual void createRenderPasses() = 0;
  virtual void createMemoryPools() = 0;
  virtual void createPipelines() = 0;
  virtual void createDescriptorPools() {};

  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_present_queue;

  std::vector<FrameResource> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  std::queue<uint32_t> m_queue_present_frames;
  Semaphore m_semaphore_draw;
  Semaphore m_semaphore_present;

  virtual void present();
  virtual void draw();
  std::atomic<bool> m_should_draw;
 private:
  virtual void render() override;
  virtual void drawLoop();
  std::thread m_thread_render;
};

#endif