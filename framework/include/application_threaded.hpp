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
#include "fence.hpp"
#include "frame_resource.hpp"
#include "semaphore.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationThreaded : public Application {
 public:
  ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  void emptyDrawQueue() override;

 private:
  virtual void render() override;
  virtual void recordDrawBuffer(FrameResource& res) override = 0;
  virtual void createCommandBuffers(FrameResource& res) = 0;
  virtual void updateCommandBuffers(FrameResource& res) = 0;
  virtual void updateDescriptors(FrameResource& resource) = 0;

  void recreatePipeline() override;

  virtual void createFramebuffers() = 0;
  virtual void createFramebufferAttachments() = 0;
  virtual void createRenderPasses() = 0;
  virtual void createMemoryPools() = 0;
  virtual void createPipelines() = 0;
  virtual void createDescriptorPools() = 0;

  virtual void drawLoop();
  virtual void draw();
  virtual void createFrameResources() = 0;

  std::thread m_thread_render;
  std::atomic<bool> m_should_draw;

 protected:
  void shut_down();
  void resize() override;
  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_record_queue;

  std::vector<FrameResource> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  Semaphore m_semaphore_draw;
  Semaphore m_semaphore_record;
};

#endif