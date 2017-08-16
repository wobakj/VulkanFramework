#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "semaphore.hpp"
#include "frame_resource.hpp"

#include "cmdline.h"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class Surface;

template<typename T>
class ApplicationThreaded : public T {
 public:
  // possibly override number of frames in abstract child classes
  ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames = 2);
  virtual ~ApplicationThreaded();

  
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

 private:
  virtual void render() override;
  virtual void drawLoop();
  std::thread m_thread_draw;
};

#include "app/application_threaded.inl"

#endif