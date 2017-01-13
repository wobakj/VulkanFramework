#include "application_threaded.hpp"

ApplicationThreaded::ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, std::vector<std::string> const& args, uint32_t num_frames) 
 :Application{resource_path, device, chain, window, args}
 ,m_frame_resources(num_frames)
 ,m_semaphore_draw{0}
 ,m_semaphore_present{num_frames}
 ,m_should_draw{true}
{}

void ApplicationThreaded::startRenderThread() {
  if (!m_thread_render.joinable()) {
    m_thread_render = std::thread(&ApplicationThreaded::drawLoop, this);
  }
}

void ApplicationThreaded::shutDown() {
  emptyDrawQueue();
  // shut down render thread
  m_should_draw = false;
  if(m_thread_render.joinable()) {
    m_semaphore_draw.shutDown();
    m_thread_render.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  for (auto const& res : m_frame_resources) {
    // reset command buffers because the draw indirect buffer counts as reference to memory
    // must be destroyed before memory is free
    for(auto const& command_buffer : res.command_buffers) {
      command_buffer.second.reset({});    
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

void ApplicationThreaded::present() {
  // get frame to present
  uint32_t frame_present = 0;
  bool can_present = false;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    if (!m_queue_present_frames.empty()) {
      // get next frame to present
      frame_present = m_queue_present_frames.front();
      m_queue_present_frames.pop();
      can_present = true;
    }   
  }

  if (can_present) {
    presentFrame(m_frame_resources.at(frame_present), m_device.getQueue("present"));
    m_queue_record_frames.push(frame_present);
  }  
}

void ApplicationThreaded::render() {
  // only calculate new frame if previous one was rendered
  m_semaphore_present.wait();
  present();

  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  uint32_t frame_record = 0;
  frame_record = m_queue_record_frames.front();
  m_queue_record_frames.pop();

  auto& resource_record = m_frame_resources.at(frame_record);

  // wait for last acquisition until acquiring again
  resource_record.fenceAcquire().wait();
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  resource_record.fenceDraw().wait();
  recordDrawBuffer(resource_record);

  // add newly recorded frame for drawing
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(frame_record);
    m_semaphore_draw.signal();
  }
}

void ApplicationThreaded::draw() {
  m_semaphore_draw.wait();
  // allow closing of application
  if (!m_should_draw) return;

  static std::uint64_t frame = 0;
  ++frame;
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    assert(!m_queue_draw_frames.empty());
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  auto& resource_draw = m_frame_resources.at(frame_draw);
  submitDraw(resource_draw);
  // make frame avaible for rerecording
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    m_queue_present_frames.push(frame_draw);
    m_semaphore_present.signal();
  }
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
  std::cout << "done with drawqueue" << std::endl;
}

void ApplicationThreaded::resize() {
  // draw queue is emptied in launcher::resize
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();

  createPipelines();
  createDescriptorPools();
  for (auto& res : m_frame_resources) {
    updateDescriptors(res);
    updateCommandBuffers(res);
  }
}

void ApplicationThreaded::recreatePipeline() {
  emptyDrawQueue();

  createPipelines();
  createDescriptorPools();
  for (auto& res : m_frame_resources) {
    updateDescriptors(res);
    updateCommandBuffers(res);
  }
}