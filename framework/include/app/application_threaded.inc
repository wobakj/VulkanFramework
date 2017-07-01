
template<typename T>
cmdline::parser ApplicationThreaded<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationThreaded<T>::ApplicationThreaded(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse, uint32_t num_frames) 
 :T{resource_path, device, surf, num_frames + 1, cmd_parse}
 ,m_frame_resources(num_frames)
 ,m_semaphore_draw{0}
 ,m_semaphore_present{num_frames}
 ,m_should_draw{true}
{
  this->m_statistics.addTimer("sema_present");
  this->m_statistics.addTimer("sema_draw");
  this->m_statistics.addTimer("fence_draw");
  this->m_statistics.addTimer("fence_draw2");
  this->m_statistics.addTimer("fence_acquire");
  this->m_statistics.addTimer("queue_present");
  this->m_statistics.addTimer("record");
  this->m_statistics.addTimer("draw");
}

template<typename T>
ApplicationThreaded<T>::~ApplicationThreaded() {
  std::cout << std::endl;
  std::cout << "Average present semaphore time: " << this->m_statistics.get("sema_present") << " milliseconds " << std::endl;
  // std::cout << "Average present queue time: " << this->m_statistics.get("queue_present") << " milliseconds " << std::endl;
  std::cout << "Average acquire fence time: " << this->m_statistics.get("fence_acquire") << " milliseconds " << std::endl;
  std::cout << "Average record time: " << this->m_statistics.get("record") << " milliseconds " << std::endl;
  std::cout << std::endl;
  std::cout << "Average draw semaphore time: " << this->m_statistics.get("sema_draw") << " milliseconds " << std::endl;
  std::cout << "Average draw fence time: " << this->m_statistics.get("fence_draw") << " milliseconds " << std::endl;
  std::cout << "Average CPU draw time: " << this->m_statistics.get("draw") << " milliseconds " << std::endl;
}

template<typename T>
void ApplicationThreaded<T>::startRenderThread() {
  if (!m_thread_draw.joinable()) {
    m_thread_draw = std::thread(&ApplicationThreaded<T>::drawLoop, this);
  }
}

template<typename T>
void ApplicationThreaded<T>::shutDown() {
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

template<typename T>
void ApplicationThreaded<T>::createFrameResources() {
  // create resources for one less image than swap chain
  // only numImages - 1 images can be acquired at a time
  for (auto& res : m_frame_resources) {
    res = std::move(this->createFrameResource());
    m_queue_record_frames.push(uint32_t(m_queue_record_frames.size()));
  }
}

template<typename T>
void ApplicationThreaded<T>::updateCommandBuffers() {
  for (auto& res : m_frame_resources) {
    this->updateResourceCommandBuffers(res);
  }
}
template<typename T>
void ApplicationThreaded<T>::updateResourcesDescriptors() {
  for (auto& res : m_frame_resources) {
    this->updateResourceDescriptors(res);
  }
}

template<typename T>
void ApplicationThreaded<T>::present() {
  // get frame to present
  auto frame_present = pullForPresent();
  // present frame if one is avaible
  if (frame_present >= 0) {
    this->presentFrame(m_frame_resources.at(frame_present));
    m_queue_record_frames.push(uint32_t(frame_present));
  }  
}

template<typename T>
void ApplicationThreaded<T>::render() {
  this->m_statistics.start("sema_present");
  // only calculate new frame if previous one was rendered
  m_semaphore_present.wait();
  this->m_statistics.stop("sema_present");
  present();
  this->m_statistics.start("record");
  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  auto frame_record = pullForRecord();
  // get resource to record
  auto& resource_record = m_frame_resources.at(frame_record);

  this->acquireImage(resource_record);
  this->recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForDraw(frame_record);
  this->m_statistics.stop("record");
}

template<typename T>
void ApplicationThreaded<T>::draw() {
  this->m_statistics.start("sema_draw");
  m_semaphore_draw.wait();
  this->m_statistics.stop("sema_draw");
  this->m_statistics.start("draw");
  // allow closing of application
  if (!m_should_draw) return;
  static std::uint64_t frame = 0;
  ++frame;
  // get frame to draw
  auto frame_draw = pullForDraw();
  // get resource to draw
  auto& resource_draw = m_frame_resources.at(frame_draw);
  this->submitDraw(resource_draw);
  // wait for drawing finish until rerecording
  this->m_statistics.start("fence_draw");
  resource_draw.fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  // make frame avaible for rerecording
  pushForPresent(frame_draw);
  this->m_statistics.stop("draw");
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
  this->m_device.getQueue("present").waitIdle();
  // wait until draw resources are avaible before recallocation
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }
  // give record queue enough signals to process all frames
  m_semaphore_present.set(uint32_t(m_frame_resources.size()));
}