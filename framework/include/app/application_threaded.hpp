#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include "app/application_single.hpp"
#include "semaphore.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

namespace cmdline {
  class parser;
}

class Surface;
class SubmitInfo;
class Device;

template<typename T>
class ApplicationThreaded : public ApplicationSingle<T> {
 public:
  // possibly override number of frames in abstract child classes
  ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames = 2);
  virtual ~ApplicationThreaded();

  virtual void emptyDrawQueue() override;
  // default parser without arguments
  static cmdline::parser getParser();
  
 protected:
  // replace shutdown of single thread app
  virtual void shutDown() override;

  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_present_queue;

  // std::vector<FrameResource> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  std::queue<uint32_t> m_queue_present_frames;
  Semaphore m_semaphore_draw;
  Semaphore m_semaphore_present;

  virtual void present();
  virtual void draw();
  std::atomic<bool> m_should_draw;

  void pushForDraw(uint32_t frame);
  uint32_t pullForRecord();
 private:
  void pushForPresent(uint32_t frame);
  uint32_t pullForDraw();
  int64_t pullForPresent();
  void startRenderThread();
  virtual void render() override;
  virtual void drawLoop();
  std::thread m_thread_draw;
};

#include "app/application_threaded.inl"

#endif