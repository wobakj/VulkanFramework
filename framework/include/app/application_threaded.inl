#include "wrap/submit_info.hpp"

#include "cmdline.h"

template<typename T>
cmdline::parser ApplicationThreaded<T>::getParser() {
  return ApplicationSingle<T>::getParser();
}

template<typename T>
ApplicationThreaded<T>::ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames) 
 :ApplicationSingle<T>{resource_path, device, surf, cmd_parse, num_frames + 1}
 ,m_semaphore_draw{0}
 ,m_semaphore_present{0}
 ,m_should_draw{true}
{
  std::cout << "using 2 threads" << std::endl;

  this->m_statistics.addTimer("sema_present");
  this->m_statistics.addTimer("sema_draw");
  this->m_statistics.addTimer("frame_draw");

  // fill record queue with references to all frame resources
  for (uint32_t i = 0; i < this->m_frame_resources.size(); ++i) {
    m_queue_record_frames.push(i);
  }
  
  startRenderThread();
}

template<typename T>
ApplicationThreaded<T>::~ApplicationThreaded() {
  std::cout << std::endl;
  std::cout << "Drawing Thread" << std::endl;
  std::cout << "Draw semaphore time: " << this->m_statistics.get("sema_draw") << " milliseconds " << std::endl;
  std::cout << "Draw frame time: " << this->m_statistics.get("frame_draw") << " milliseconds " << std::endl;
  std::cout << "Present semaphore time: " << this->m_statistics.get("sema_present") << " milliseconds " << std::endl;
}

template<typename T>
void ApplicationThreaded<T>::startRenderThread() {
  m_thread_draw = std::thread(&ApplicationThreaded<T>::drawLoop, this);
}

// replace shutdown of single thread app
template<typename T>
void ApplicationThreaded<T>::shutDown() {
  emptyDrawQueue();
  // shut down drawing thread
  m_should_draw = false;
  m_semaphore_draw.shutDown();
  m_thread_draw.join();
  for (auto const& res : this->m_frame_resources) {
    // reset command buffers because the draw indirect buffer counts as reference to memory
    // must be destroyed before memory is freed
    for(auto const& command_buffer : res.command_buffers) {
      command_buffer.second->reset({});    
    }
  }
}

template<typename T>
void ApplicationThreaded<T>::present() {
  // present next frame
  auto& resource_present = pullForPresent();
  this->presentFrame(resource_present);
  m_queue_record_frames.push(resource_present.index);
}

template<typename T>
void ApplicationThreaded<T>::render() {
  this->m_statistics.start("record");
  static uint64_t frame = 0;
  ++frame;
  // get next resource to record
  auto& resource_record = pullForRecord();
  // make sure no command buffer is in use
  this->m_statistics.start("fence_draw");
  resource_record.fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  // transfer doesnt need to know about image
  this->recordTransferBuffer(resource_record);
  // draw needs image
  this->acquireImage(resource_record);
  this->recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForDraw(resource_record);
  this->m_statistics.stop("record");
  
  this->m_statistics.start("sema_present");
  // only calculate new frame if previous one was rendered
  m_semaphore_present.wait();
  this->m_statistics.stop("sema_present");
  present();
}

template<typename T>
void ApplicationThreaded<T>::draw() {
  this->m_statistics.start("frame_draw");
  static std::uint64_t frame = 0;
  ++frame;
  // get next resource to draw
  auto& resource_draw = pullForDraw();
  this->submitDraw(resource_draw);
  // do not wait here for finishing anymore
  // make frame avaible for rerecording
  pushForPresent(resource_draw);
  this->m_statistics.stop("frame_draw");
}

template<typename T>
void ApplicationThreaded<T>::pushForDraw(FrameResource& resource) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(resource.index);
  }
  m_semaphore_draw.signal();
}

template<typename T>
void ApplicationThreaded<T>::pushForPresent(FrameResource& resource) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    m_queue_present_frames.push(resource.index);
  }
  m_semaphore_present.signal();
}

template<typename T>
FrameResource& ApplicationThreaded<T>::pullForRecord() {
  // get next frame to record
  uint32_t frame_record = 0;
  assert(!m_queue_record_frames.empty());
  frame_record = m_queue_record_frames.front();
  // std::cout << m_queue_record_frames.size() << std::endl;
  m_queue_record_frames.pop();
  return this->m_frame_resources[frame_record];
}

template<typename T>
FrameResource& ApplicationThreaded<T>::pullForDraw() {
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    assert(!m_queue_draw_frames.empty());
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  return this->m_frame_resources[frame_draw];
}

template<typename T>
FrameResource& ApplicationThreaded<T>::pullForPresent() {
  uint32_t frame_present = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    assert(!m_queue_present_frames.empty());
    // get next frame to present
    frame_present = m_queue_present_frames.front();
    m_queue_present_frames.pop();
  }
  return this->m_frame_resources[frame_present];
}

template<typename T>
void ApplicationThreaded<T>::drawLoop() {
  // wait for first frame
  m_semaphore_draw.wait();
  while (m_should_draw) {
    draw();
    this->m_statistics.start("sema_draw");
    m_semaphore_draw.wait();
    this->m_statistics.stop("sema_draw");
  }
}

template<typename T>
void ApplicationThreaded<T>::emptyDrawQueue() {
  // check if all frames are ready for recording
  while(m_queue_record_frames.size() < this->m_frame_resources.size()) {
    // check if frames can be presented
    size_t num_present = 0;
    {
      std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
      num_present = m_queue_present_frames.size();
    }
    // invalidate remaining frames
    while (num_present > 0) {
      m_semaphore_present.wait(); 
      m_queue_record_frames.push(pullForPresent().index);
      --num_present;
    }
  }
  // MUST wait after every present or everything freezes
  this->m_device.getQueue("present").waitIdle();
  // wait until draw resources are avaible before recallocation
  for (auto const& res : this->m_frame_resources) {
    res.waitFences();
  }
}
