#include "app/application_threaded.hpp"

cmdline::parser ApplicationThreaded::getParser() {
  return ApplicationWin::getParser();
}

ApplicationThreaded::ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames) 
 :ApplicationWin{resource_path, device, surf, 3, cmd_parse}
 ,m_frame_resources(num_frames)
 ,m_semaphore_draw{0}
 ,m_semaphore_present{num_frames}
 ,m_should_draw{true}
{
  m_statistics.addTimer("sema_present");
  m_statistics.addTimer("sema_draw");
  m_statistics.addTimer("fence_draw");
  m_statistics.addTimer("fence_draw2");
  m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("queue_present");
  m_statistics.addTimer("record");
  m_statistics.addTimer("draw");
}

ApplicationThreaded::~ApplicationThreaded() {
  std::cout << std::endl;
  std::cout << "Average present semaphore time: " << m_statistics.get("sema_present") << " milliseconds " << std::endl;
  std::cout << "Average present queue time: " << m_statistics.get("queue_present") << " milliseconds " << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds " << std::endl;
  std::cout << "Average record time: " << m_statistics.get("record") << " milliseconds " << std::endl;
  std::cout << std::endl;
  std::cout << "Average draw semaphore time: " << m_statistics.get("sema_draw") << " milliseconds " << std::endl;
  std::cout << "Average draw fence time: " << m_statistics.get("fence_draw") << " milliseconds " << std::endl;
  std::cout << "Average CPU draw time: " << m_statistics.get("draw") << " milliseconds " << std::endl;
}

void ApplicationThreaded::startRenderThread() {
  if (!m_thread_draw.joinable()) {
    m_thread_draw = std::thread(&ApplicationThreaded::drawLoop, this);
  }
}

void ApplicationThreaded::shutDown() {
  emptyDrawQueue();
  // shut down render thread
  m_should_draw = false;
  if(m_thread_draw.joinable()) {
    m_semaphore_draw.shutDown();
    m_thread_draw.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  for (auto const& res : m_frame_resources) {
    // reset command buffers because the draw indirect buffer counts as reference to memory
    // must be destroyed before memory is free
    for(auto const& command_buffer : res.command_buffers) {
      command_buffer.second->reset({});    
    }
  }
}

void ApplicationThreaded::createFrameResources() {
  // create resources for one less image than swap chain
  // only numImages - 1 images can be acquired at a time
  for (auto& res : m_frame_resources) {
    res = std::move(createFrameResource());
    m_queue_record_frames.push(uint32_t(m_queue_record_frames.size()));
  }
}

void ApplicationThreaded::updateCommandBuffers() {
  for (auto& res : m_frame_resources) {
    updateResourceCommandBuffers(res);
  }
}
void ApplicationThreaded::updateResourcesDescriptors() {
  for (auto& res : m_frame_resources) {
    updateResourceDescriptors(res);
  }
}

void ApplicationThreaded::present() {
  // get frame to present
  auto frame_present = pullForPresent();
  // present frame if one is avaible
  if (frame_present >= 0) {
    m_statistics.start("queue_present");
    presentFrame(m_frame_resources.at(frame_present), m_device.getQueue("present"));
    m_statistics.stop("queue_present");
    m_queue_record_frames.push(uint32_t(frame_present));
  }  
}

void ApplicationThreaded::render() {
  m_statistics.start("sema_present");
  // only calculate new frame if previous one was rendered
  m_semaphore_present.wait();
  m_statistics.stop("sema_present");
  present();
  m_statistics.start("record");
  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  auto frame_record = pullForRecord();
  // get resource to record
  auto& resource_record = m_frame_resources.at(frame_record);
  // wait for last acquisition until acquiring again
  m_statistics.start("fence_acquire");
  resource_record.fence("acquire").wait();
  m_statistics.stop("fence_acquire");
  acquireImage(resource_record);
  recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForDraw(frame_record);
  m_statistics.stop("record");
}

void ApplicationThreaded::draw() {
  m_statistics.start("sema_draw");
  m_semaphore_draw.wait();
  m_statistics.stop("sema_draw");
  m_statistics.start("draw");
  // allow closing of application
  if (!m_should_draw) return;
  static std::uint64_t frame = 0;
  ++frame;
  // get frame to draw
  auto frame_draw = pullForDraw();
  // get resource to draw
  auto& resource_draw = m_frame_resources.at(frame_draw);
  submitDraw(resource_draw);
  // wait for drawing finish until rerecording
  m_statistics.start("fence_draw");
  resource_draw.fence("draw").wait();
  m_statistics.stop("fence_draw");
  // make frame avaible for rerecording
  pushForPresent(frame_draw);
  m_statistics.stop("draw");
}

void ApplicationThreaded::pushForDraw(uint32_t frame) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(frame);
  }
  m_semaphore_draw.signal();
}

void ApplicationThreaded::pushForPresent(uint32_t frame) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    m_queue_present_frames.push(frame);
  }
  m_semaphore_present.signal();
}

uint32_t ApplicationThreaded::pullForRecord() {
  // get next frame to record
  uint32_t frame_record = 0;
  frame_record = m_queue_record_frames.front();
  // std::cout << m_queue_record_frames.size() << std::endl;
  m_queue_record_frames.pop();
  return frame_record;
}

uint32_t ApplicationThreaded::pullForDraw() {
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

int64_t ApplicationThreaded::pullForPresent() {
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

void ApplicationThreaded::drawLoop() {
  while (m_should_draw) {
    draw();
  }
}

void ApplicationThreaded::emptyDrawQueue() {
  // render remaining recorded frames
  size_t num_record_frames = 0;
  while(num_record_frames < m_frame_resources.size()) {
    // check if all frames are ready for recording
    num_record_frames = m_queue_record_frames.size();
    if (num_record_frames == m_frame_resources.size()) {
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
  m_device.getQueue("present").waitIdle();
  // wait until draw resources are avaible before recallocation
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }
  // give record queue enough signals to process all frames
  m_semaphore_present.set(uint32_t(m_frame_resources.size()));
}
