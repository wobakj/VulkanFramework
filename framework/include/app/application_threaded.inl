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
  // get frame to present
  auto frame_present = pullForPresent();
  // present frame if one is avaible
  if (frame_present >= 0) {
    this->presentFrame(this->m_frame_resources.at(frame_present));
    m_queue_record_frames.push(uint32_t(frame_present));
  }  
  //TODO: is the if necessary? 
}

template<typename T>
void ApplicationThreaded<T>::render() {
  this->m_statistics.start("record");
  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  auto frame_record = pullForRecord();
  // get resource to record
  auto& resource_record = this->m_frame_resources.at(frame_record);
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
  pushForDraw(frame_record);
  this->m_statistics.stop("record");
  
  this->m_statistics.start("sema_present");
  // only calculate new frame if previous one was rendered
  m_semaphore_present.wait();
  this->m_statistics.stop("sema_present");
  present();
}

template<typename T>
void ApplicationThreaded<T>::draw() {
  this->m_statistics.start("sema_draw");
  m_semaphore_draw.wait();
  this->m_statistics.stop("sema_draw");
  this->m_statistics.start("frame_draw");
  // allow closing of application
  if (!m_should_draw) return;
  static std::uint64_t frame = 0;
  ++frame;
  // get frame to draw
  auto frame_draw = pullForDraw();
  // get resource to draw
  auto& resource_draw = this->m_frame_resources.at(frame_draw);
  this->submitDraw(resource_draw);
  // do not wait here for finishing anymore
  // make frame avaible for rerecording
  pushForPresent(frame_draw);
  this->m_statistics.stop("frame_draw");
}

template<typename T>
void ApplicationThreaded<T>::pushForDraw(uint32_t frame) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(frame);
  }
  m_semaphore_draw.signal();
}

template<typename T>
void ApplicationThreaded<T>::pushForPresent(uint32_t frame) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    m_queue_present_frames.push(frame);
  }
  m_semaphore_present.signal();
}

template<typename T>
uint32_t ApplicationThreaded<T>::pullForRecord() {
  // get next frame to record
  uint32_t frame_record = 0;
  frame_record = m_queue_record_frames.front();
  // std::cout << m_queue_record_frames.size() << std::endl;
  m_queue_record_frames.pop();
  return frame_record;
}

template<typename T>
uint32_t ApplicationThreaded<T>::pullForDraw() {
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    assert(!m_queue_draw_frames.empty());
    // std::cout << m_queue_draw_frames.size() << std::endl;
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  return frame_draw;
}

template<typename T>
int64_t ApplicationThreaded<T>::pullForPresent() {
  int64_t frame_present = -1;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    if (!m_queue_present_frames.empty()) {
      // get next frame to present
      frame_present = m_queue_present_frames.front();
      m_queue_present_frames.pop();
    }   
  }
  return frame_present;
}

template<typename T>
void ApplicationThreaded<T>::drawLoop() {
  while (m_should_draw) {
    draw();
  }
}

template<typename T>
void ApplicationThreaded<T>::emptyDrawQueue() {
  // render remaining recorded frames
  size_t num_record_frames = 0;
  while(num_record_frames < this->m_frame_resources.size()) {
    // check if all frames are ready for recording
    num_record_frames = m_queue_record_frames.size();
    if (num_record_frames == this->m_frame_resources.size()) {
      break;
    }
    // check if frames can be presented
    int64_t num_present = 0;
    {
      std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
      num_present = m_queue_present_frames.size();
    }
    // present if possible
    while (num_present > 0) {
      m_semaphore_present.wait(); 
      present();
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
