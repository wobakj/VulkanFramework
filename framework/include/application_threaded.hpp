#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "semaphore.hpp"
#include "frame_resource.hpp"
#include "statistics.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationThreaded : public Application {
 public:
  ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse, uint32_t num_frames);
  void emptyDrawQueue() override;
  virtual ~ApplicationThreaded();

 protected:
  void createFrameResources();
  virtual FrameResource createFrameResource() override = 0;
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
  void pushForDraw(uint32_t frame);
  void pushForPresent(uint32_t frame);
  uint32_t pullForRecord();
  uint32_t pullForDraw();
  int64_t pullForPresent();

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
  Statistics m_statistics;

 private:
  virtual void render() override;
  virtual void drawLoop();
  std::thread m_thread_render;
};

#endif