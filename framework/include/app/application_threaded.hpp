#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "app/application_win.hpp"
#include "semaphore.hpp"
#include "frame_resource.hpp"
#include "statistics.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class Surface;

class ApplicationThreaded : public ApplicationWin {
 public:
  // possibly override number of frames in abstract child classes
  ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames = imageCount - 1);
  virtual ~ApplicationThreaded();

  static const uint32_t imageCount;
  void emptyDrawQueue() override;
  // default parser without arguments
  static cmdline::parser getParser();
  
 protected:
  void createFrameResources() override;
  virtual void shutDown();
  void updateCommandBuffers() override;
  void updateResourcesDescriptors() override;

  void startRenderThread();

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
  std::thread m_thread_draw;
};

#endif