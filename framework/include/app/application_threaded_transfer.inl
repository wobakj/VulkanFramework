#include <wrap/submit_info.hpp>

template<typename T> 
cmdline::parser ApplicationThreadedTransfer<T>::getParser() {
  return ApplicationThreaded<T>::getParser();
}

template<typename T> 
ApplicationThreadedTransfer<T>::ApplicationThreadedTransfer(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded<T>{resource_path, device, surf, cmd_parse, 3}
 ,m_semaphore_transfer{0}
 ,m_should_transfer{true}
{
  std::cout << "using transfer thread" << std::endl;

  this->m_statistics.addTimer("fence_transfer");
  this->m_statistics.addTimer("sema_transfer");
  this->m_statistics.addTimer("frame_transfer");

  startTransferThread();
}

template<typename T> 
ApplicationThreadedTransfer<T>::~ApplicationThreadedTransfer() {
  std::cout << std::endl;
  std::cout << "Transfer Thread" << std::endl;
  std::cout << "Transfer semaphore time: " << this->m_statistics.get("sema_transfer") << " milliseconds " << std::endl;
  std::cout << "Transfer fence time: " << this->m_statistics.get("fence_transfer") << " milliseconds " << std::endl;
  std::cout << "Transfer frame time: " << this->m_statistics.get("frame_transfer") << " milliseconds " << std::endl;
}

template<typename T> 
void ApplicationThreadedTransfer<T>::startTransferThread() {
  m_thread_transfer = std::thread(&ApplicationThreadedTransfer<T>::transferLoop, this);
}

template<typename T> 
void ApplicationThreadedTransfer<T>::shutDown() {
  // shut down transfer thread
  m_should_transfer = false;
  m_semaphore_transfer.shutDown();
  m_thread_transfer.join();
  this->m_device.getQueue("transfer").waitIdle();
  // shut down drawing thread
 ApplicationThreaded<T>::shutDown();
}

template<typename T> 
FrameResource ApplicationThreadedTransfer<T>::createFrameResource() {
  auto res = ApplicationThreaded<T>::createFrameResource();
  // overwrite transfer cb from ApplicationSingle, to use different queue
  res.setCommandBuffer("transfer", this->m_command_pools.at("transfer").createBuffer(vk::CommandBufferLevel::ePrimary));  
  res.addSemaphore("transfer");
  res.addFence("transfer");
  // record once to prevent validation error when not used 
  res.commandBuffer("transfer")->begin(vk::CommandBufferBeginInfo{});
  res.commandBuffer("transfer")->end();
  return res;
}

template<typename T> 
void ApplicationThreadedTransfer<T>::render() {
  this->m_statistics.start("record");
  static uint64_t frame = 0;
  ++frame;
  // get next resource to record
  auto& resource_record = this->pullForRecord();
  // wait for previous transfer completion
  this->m_statistics.start("fence_transfer");
  resource_record.fence("transfer").wait();
  this->m_statistics.stop("fence_transfer");
  // transfer doesnt need to know about image
  this->recordTransferBuffer(resource_record);
  // draw needs image
  this->acquireImage(resource_record);
  // wait for previous draw completion
  this->m_statistics.start("fence_draw");
  resource_record.fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  this->recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForTransfer(resource_record);
  this->m_statistics.stop("record");

  this->present();
}

template<typename T> 
void ApplicationThreadedTransfer<T>::transfer() {
  this->m_statistics.start("frame_transfer");
  static std::uint64_t frame = 0;
  ++frame;
  // get frame to transfer
  auto& resource_transfer = pullForTransfer();
  // get resource to transfer
  submitTransfer(resource_transfer);
  // make frame avaible for rerecording
  this->pushForDraw(resource_transfer);
  this->m_statistics.stop("frame_transfer");
}

template<typename T> 
void ApplicationThreadedTransfer<T>::pushForTransfer(FrameResource& resource) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_transfer_queue};
    m_queue_transfer_frames.push(resource.index);
  }
  m_semaphore_transfer.signal();
}

template<typename T> 
FrameResource& ApplicationThreadedTransfer<T>::pullForTransfer() {
  uint32_t frame_transfer = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_transfer_queue};
    assert(!m_queue_transfer_frames.empty());
    // get frame to transfer
    frame_transfer = m_queue_transfer_frames.front();
    m_queue_transfer_frames.pop();
  }
  return this->m_frame_resources[frame_transfer];
}

template<typename T> 
void ApplicationThreadedTransfer<T>::submitTransfer(FrameResource& res) {
  SubmitInfo info{};
  info.addCommandBuffer(res.command_buffers.at("transfer").get());
  info.addSignalSemaphore(res.semaphore("transfer"));

  res.fences.at("transfer").reset();
  this->m_device.getQueue("transfer").submit({info}, res.fences.at("transfer"));
}

template<typename T> 
SubmitInfo ApplicationThreadedTransfer<T>::createDrawSubmitInfo(FrameResource const& res) const {
  // ignore submit info of ApplicationThreaded
  SubmitInfo info = T::createDrawSubmitInfo(res);
  info.addWaitSemaphore(res.semaphore("transfer"), vk::PipelineStageFlagBits::eDrawIndirect);
  return info;
}

template<typename T> 
void ApplicationThreadedTransfer<T>::transferLoop() {
  // wait for first frame
  m_semaphore_transfer.wait();
  while (m_should_transfer) {
    transfer();
    this->m_statistics.start("sema_transfer");
    m_semaphore_transfer.wait();
    this->m_statistics.stop("sema_transfer");
  }
}
